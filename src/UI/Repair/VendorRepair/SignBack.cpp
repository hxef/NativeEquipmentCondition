#include "UI/Repair/VendorRepair/SignBack.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/TraceLog/TraceLog.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace VendorRepair::SignBack
{
	namespace
	{
		// -------------------------------------------------------------------
		// The targets
		// -------------------------------------------------------------------

		// 1 target, with the screen and the movie it was made for, since a
		// new screen can sit where a gone one sat, and the trader the trace
		// names when it goes. At exit the movies are gone first, so a target
		// left then is let go of.
		struct Kept
		{
			const RE::BarterMenu*                    menu = nullptr;
			const Scaleform::GFx::Movie*             movie = nullptr;
			std::unique_ptr<RE::BSGFxShaderFXTarget> target;
			std::string                              trader;

			Kept() = default;
			Kept(const Kept&) = delete;
			Kept& operator=(const Kept&) = delete;
			~Kept() { static_cast<void>(target.release()); }

			[[nodiscard]] bool Of(const RE::BarterMenu& a_menu) const { return target && menu == &a_menu && movie == a_menu.uiMovie.get(); }
		};

		std::mutex           g_lock;
		std::array<Kept, 2>  g_kept;
		constexpr std::array TARGETS{ Target::kBack, Target::kColor };

		[[nodiscard]] Kept& KeptOf(Target a_target)
		{
			return g_kept[static_cast<std::size_t>(a_target)];
		}

		// What the trace calls each target.
		[[nodiscard]] std::string_view Named(Target a_target)
		{
			return a_target == Target::kBack ? "the list's background"sv : "its own HUD colour"sv;
		}

		// Stops the target listening and lets go of it without deleting it.
		void LetGo(std::unique_ptr<RE::BSGFxShaderFXTarget> a_target)
		{
			if (!a_target) {
				return;
			}
			if (const auto source = RE::ApplyColorUpdateEvent::GetEventSource()) {
				source->UnregisterSink(a_target.get());
			}
			static_cast<void>(a_target.release());
		}

		// -------------------------------------------------------------------
		// The barter screen's delete
		// -------------------------------------------------------------------

		REL::Relocation<void* (*)(RE::BarterMenu*, std::uint32_t)> _DeleteMenu;

		// The delete hook's slot. Once a recheck finds a DLL there that skips
		// NEC, Held is false and no target is made again.
		CallPatch::LinkBase g_deleteLink;

		// Runs before the screen's own destructor, so the movie still exists
		// when a target releases its clip. It runs whatever bNecOn says.
		void* DeleteMenuHk(RE::BarterMenu* a_menu, std::uint32_t a_flags)
		{
			if (a_menu) {
				Drop(*a_menu, "as the barter screen goes"sv);
			}
			return _DeleteMenu(a_menu, a_flags);
		}
	}

	void Install()
	{
		// No mod takes anything here, see Part::kTraderSignBack. Shared while
		// the call still reaches NEC.
		const auto& menu = RE::BarterMenu::VTABLE[0];
		_DeleteMenu = CallPatch::PatchSlot(menu, { 0x00, 0x00 }, DeleteMenuHk, "barter screen delete", Part::kTraderSignBack, true, &g_deleteLink).value_or(0);
	}

	bool Held()
	{
		return g_deleteLink.Holds();
	}

	bool Make(RE::BarterMenu& a_menu, const Scaleform::GFx::Value& a_clip, std::string_view a_trader, Target a_target)
	{
		if (!Held()) {
			return false;
		}
		std::unique_ptr<RE::BSGFxShaderFXTarget> old;
		{
			const std::scoped_lock l(g_lock);
			auto&                  kept = KeptOf(a_target);
			if (kept.Of(a_menu)) {
				return true;
			}
			old = std::move(kept.target);
		}
		// Left by a screen that went without NEC's delete.
		LetGo(std::move(old));

		// As the barter screen's constructor gives its own bracket clip the
		// list's background, no colour at full size, and its sides the
		// gameplay HUD colour.
		auto target = std::make_unique<RE::BSGFxShaderFXTarget>(a_clip);
		if (a_target == Target::kBack) {
			target->EnableShadedBackground(RE::HUDColorTypes::kMenuNoColorBackground, 1.0F);
		} else {
			target->SetToHUDColor(false);
		}
		{
			const RE::BSAutoWriteLock l(a_menu.cachedQuadsLock);
			a_menu.shaderFXObjects.push_back(target.get());
		}
		{
			const std::scoped_lock l(g_lock);
			auto&                  kept = KeptOf(a_target);
			kept.menu = &a_menu;
			kept.movie = a_menu.uiMovie.get();
			kept.target = std::move(target);
			kept.trader = a_trader;
		}
		TraceLog::Line("menu", "{:s}'s sign gets {:s}", a_trader, Named(a_target));
		return true;
	}

	void Drop(RE::BarterMenu& a_menu, std::string_view a_how, Target a_target)
	{
		std::unique_ptr<RE::BSGFxShaderFXTarget> target;
		std::string                              trader;
		{
			const std::scoped_lock l(g_lock);
			auto&                  kept = KeptOf(a_target);
			if (!kept.Of(a_menu)) {
				return;
			}
			target = std::move(kept.target);
			trader = std::move(kept.trader);
			kept.menu = nullptr;
			kept.movie = nullptr;
		}
		// Out of the list first, under the lock the engine reads it under,
		// then deleted while the movie still exists.
		{
			const RE::BSAutoWriteLock l(a_menu.cachedQuadsLock);
			auto&                     list = a_menu.shaderFXObjects;
			if (const auto it = std::find(list.begin(), list.end(), target.get()); it != list.end()) {
				list.erase(it);
			}
		}
		target.reset();
		TraceLog::Line("menu", "{:s}'s sign lets go of {:s} {:s}", trader, Named(a_target), a_how);
	}

	void Drop(RE::BarterMenu& a_menu, std::string_view a_how)
	{
		for (const auto target : TARGETS) {
			Drop(a_menu, a_how, target);
		}
	}

	void ForgetOld()
	{
		{
			const std::scoped_lock l(g_lock);
			if (std::ranges::none_of(g_kept, [](const Kept& a_kept) { return a_kept.target != nullptr; })) {
				return;
			}
		}
		// Held past the lock, since its last reference runs DeleteMenuHk,
		// which takes it.
		const auto* ui = RE::UI::GetSingleton();
		const auto  live = ui ? ui->GetMenu<RE::BarterMenu>() : nullptr;

		for (const auto kind : TARGETS) {
			std::unique_ptr<RE::BSGFxShaderFXTarget> old;
			std::string                              trader;
			{
				const std::scoped_lock l(g_lock);
				auto&                  kept = KeptOf(kind);
				if (!kept.target || (live && kept.Of(*live))) {
					continue;
				}
				old = std::move(kept.target);
				trader = std::move(kept.trader);
				kept.menu = nullptr;
				kept.movie = nullptr;
			}
			LetGo(std::move(old));
			TraceLog::Line("menu", "{:s}'s screen went without NEC's delete, so its sign lets go of {:s}", trader, Named(kind));
		}
	}
}
