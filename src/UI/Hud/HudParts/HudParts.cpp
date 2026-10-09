#include "UI/Hud/HudParts/HudParts.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "UI/Flash.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <format>
#include <memory>
#include <mutex>
#include <vector>

namespace HudParts
{
	namespace
	{
		// The colour targets AddColorTarget made, with the HUD menu each was
		// made for.
		struct ColorTarget
		{
			const RE::HUDMenu*                       menu;
			std::unique_ptr<RE::BSGFxShaderFXTarget> target;
		};

		// At exit the movies are gone first, so the targets left are let go of.
		struct ColorTargets : std::vector<ColorTarget>
		{
			~ColorTargets()
			{
				for (auto& target : *this) {
					static_cast<void>(target.target.release());
				}
			}
		};

		std::mutex   g_colorTargetsLock;
		ColorTargets g_colorTargets;

		REL::Relocation<void* (*)(RE::HUDMenu*, std::uint32_t)> _DeleteMenu;

		// The delete hook's slot. It is shared with a DLL that hands the call
		// on, which may still skip it now and then, see ForgetOldTargets. Once
		// a recheck finds a DLL there that skips NEC, the hook never runs
		// again, see DropTargets.
		CallPatch::LinkBase g_deleteLink;

		// Runs before the menu's own destructor, so the movie still exists when
		// a target releases its clip. The last reference to a menu can be
		// released on any thread, hence the lock.
		void* DeleteMenuHk(RE::HUDMenu* a_menu, std::uint32_t a_flags)
		{
			std::vector<std::unique_ptr<RE::BSGFxShaderFXTarget>> gone;
			{
				const std::scoped_lock l(g_colorTargetsLock);
				for (auto it = g_colorTargets.begin(); it != g_colorTargets.end();) {
					if (it->menu == a_menu) {
						gone.push_back(std::move(it->target));
						it = g_colorTargets.erase(it);
					} else {
						++it;
					}
				}
			}

			if (!gone.empty()) {
				{
					const RE::BSAutoWriteLock l(a_menu->cachedQuadsLock);
					auto& list = a_menu->shaderFXObjects;
					for (const auto& target : gone) {
						if (const auto it = std::find(list.begin(), list.end(), target.get()); it != list.end()) {
							list.erase(it);
						}
					}
				}
				TraceLog::Line("menu", "HUD menu goes, and the {:d} colour targets of its CND parts with it", gone.size());
				gone.clear();
			}
			return _DeleteMenu(a_menu, a_flags);
		}

		// Once the delete hook's slot is lost, the live HUD menu's targets are
		// deleted at once, on the HUD's own thread while its movie still
		// exists. The targets of a menu already gone are let go of first.
		void DropTargets()
		{
			if (g_deleteLink.Live()) {
				return;
			}
			ForgetOldTargets();

			std::vector<ColorTarget> all;
			{
				const std::scoped_lock l(g_colorTargetsLock);
				all.swap(g_colorTargets);
			}
			if (all.empty()) {
				return;
			}

			// The menu can go between the 2 looks, so its targets are let go
			// of here too.
			const auto* ui = RE::UI::GetSingleton();
			const auto  live = ui ? ui->GetMenu<RE::HUDMenu>() : nullptr;
			for (auto& target : all) {
				if (!live || target.menu != live.get()) {
					if (const auto source = RE::ApplyColorUpdateEvent::GetEventSource()) {
						source->UnregisterSink(target.target.get());
					}
					static_cast<void>(target.target.release());
					continue;
				}
				const RE::BSAutoWriteLock l(live->cachedQuadsLock);
				auto&                     list = live->shaderFXObjects;
				if (const auto it = std::find(list.begin(), list.end(), target.target.get()); it != list.end()) {
					list.erase(it);
				}
			}
			TraceLog::Line("menu", "HUD menu delete is with another mod, so the {:d} colour targets of the CND parts go now", all.size());
		}

		// The sweep runs again a frame after a HUD menu closes, at a save load
		// or on the way to the main menu. So a menu UI let go of as it closed
		// loses its targets before the player can reach a HUD colour setting,
		// in whatever order the main menu loads. Only queued here, since the
		// game sends this while it works on its menus.
		class CloseSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			F4_HEAP_REDEFINE_NEW(CloseSink);

		private:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (!a_event.opening && a_event.menuName == RE::HUDMenu::MENU_NAME) {
					if (const auto* tasks = F4SE::GetTaskInterface()) {
						tasks->AddTask([] { ForgetOldTargets(); });
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void ForgetOldTargets()
	{
		// UI is asked only when there is something to let go of, since
		// DropTargets runs every frame once the place is cut.
		{
			const std::scoped_lock l(g_colorTargetsLock);
			if (g_colorTargets.empty()) {
				return;
			}
		}

		// Held past the lock, since its last reference runs DeleteMenuHk,
		// which takes it.
		const auto* ui = RE::UI::GetSingleton();
		const auto  live = ui ? ui->GetMenu<RE::HUDMenu>() : nullptr;

		std::vector<ColorTarget> old;
		{
			const std::scoped_lock l(g_colorTargetsLock);
			for (auto it = g_colorTargets.begin(); it != g_colorTargets.end();) {
				if (!live || it->menu != live.get()) {
					old.push_back(std::move(*it));
					it = g_colorTargets.erase(it);
				} else {
					++it;
				}
			}
		}
		if (old.empty()) {
			return;
		}

		const auto source = RE::ApplyColorUpdateEvent::GetEventSource();
		for (auto& target : old) {
			if (source) {
				source->UnregisterSink(target.target.get());
			}
			static_cast<void>(target.target.release());
		}
		TraceLog::Line("menu", "HUD menu went without NEC's delete, so the {:d} colour targets of its CND parts are let go of", old.size());
	}

	namespace Weapon
	{
		namespace
		{
			// The latest condition, and whether a weapon is drawn, written by
			// the task and read by the HUD's frames.
			std::atomic<std::int32_t> g_percent{ NONE };
			std::atomic<bool>         g_drawn{ false };

			// Set while a check waits in the task queue.
			std::atomic<bool> g_queued{ false };
		}

		void Queue()
		{
			DropTargets();
			if (!Settings::bHudCondition.GetValue() || g_queued.exchange(true)) {
				return;
			}

			const auto* tasks = F4SE::GetTaskInterface();
			if (!tasks) {
				g_queued = false;
				return;
			}

			// F4SE runs the task a frame later, on a worker thread during play,
			// so the inventory can change under it. Equipped::TryWeaponHealth
			// holds the inventory lock while it reads and returns nothing while
			// the inventory is busy, which keeps the last condition for a
			// frame.
			tasks->AddTask([] {
				auto*      player = RE::PlayerCharacter::GetSingleton();
				const auto health = Equipped::TryWeaponHealth(player);
				if (health) {
					g_percent = *health < 0.0F ? NONE : static_cast<std::int32_t>(Condition::WholePercent(*health));
				} else {
					TraceLog::Once("menu", "HUD CND kept its last reading, another thread had the player's inventory");
				}
				g_drawn = player && player->GetWeaponMagicDrawn();
				g_queued = false;
			});
		}

		std::int32_t Percent()
		{
			return Settings::bHudCondition.GetValue() ? g_percent.load() : NONE;
		}

		bool Drawn()
		{
			return g_drawn.load();
		}

		void Forget()
		{
			g_percent = NONE;
			g_drawn = false;
		}
	}

	void SetStyledText(Value& a_field, std::string_view a_text, std::int32_t a_size)
	{
		const auto html = std::format(R"(<font face="{:s}" size="{}" color="#FFFFFF">{:s}</font>)", FONT_NAME, a_size,
			Flash::HtmlEscaped(a_text));
		a_field.SetMember("htmlText"sv, Value(std::string_view{ html }));
	}

	void SetTranslatedText(Value& a_field, const char* a_key, std::int32_t a_size)
	{
		a_field.SetMember("text"sv, Value(a_key));
		SetStyledText(a_field, Flash::String(a_field, "text"sv), a_size);
	}

	void PutBox(Value& a_box, double a_x, double a_y, double a_width, double a_height)
	{
		a_box.SetMember("x"sv, Value(a_x));
		a_box.SetMember("y"sv, Value(a_y));
		a_box.SetMember("scaleX"sv, Value(std::max(a_width, 0.0)));
		a_box.SetMember("scaleY"sv, Value(std::max(a_height, 0.0)));
	}

	bool AddBox(Scaleform::GFx::Movie& a_movie, Value& a_parent, const char* a_name)
	{
		Value box;
		a_movie.CreateObject(&box, "flash.display.Sprite");
		if (!box.IsDisplayObject()) {
			return false;
		}

		Value graphics;
		if (!box.GetMember("graphics"sv, &graphics) ||
			!graphics.Invoke("beginFill", std::array{ Value(0xFFFFFF), Value(1.0) }) ||
			!graphics.Invoke("drawRect", std::array{ Value(0.0), Value(0.0), Value(1.0), Value(1.0) }) ||
			!graphics.Invoke("endFill")) {
			return false;
		}

		box.SetMember("name"sv, Value(a_name));
		return a_parent.Invoke("addChild", std::array{ box });
	}

	bool AddField(Scaleform::GFx::Movie& a_movie, Value& a_parent, const char* a_name)
	{
		Value field;
		a_movie.CreateObject(&field, "flash.text.TextField");
		if (!field.IsDisplayObject()) {
			return false;
		}

		field.SetMember("name"sv, Value(a_name));

		// The HUD font lives in the game's font library, which is what
		// embedFonts asks for.
		field.SetMember("embedFonts"sv, Value(true));
		field.SetMember("selectable"sv, Value(false));
		field.SetMember("mouseEnabled"sv, Value(false));
		field.SetMember("autoSize"sv, Value("left"));
		return a_parent.Invoke("addChild", std::array{ field });
	}

	RE::HUDMenu* MenuFor(const Scaleform::GFx::Movie& a_movie)
	{
		const auto* ui = RE::UI::GetSingleton();
		const auto  menu = ui ? ui->GetMenu<RE::HUDMenu>() : nullptr;
		return menu && menu->uiMovie.get() == &a_movie ? menu.get() : nullptr;
	}

	void Install()
	{
		// This deletes NEC's colour targets before the HUD menu goes. A DLL
		// over it that skips NEC now and then leaves a menu's targets behind,
		// and the sweep lets go of them, see ForgetOldTargets. So the place
		// stands alone, shared while the call still reaches NEC.
		REL::Relocation<std::uintptr_t> menu{ RE::HUDMenu::VTABLE[0] };
		_DeleteMenu = CallPatch::PatchSlot(menu, 0x00, DeleteMenuHk, "HUD menu delete", Part::kNone, true, &g_deleteLink).value_or(0);
	}

	void Load()
	{
		// Once, since UI lasts as long as the game.
		static bool registered = false;
		auto*       ui = RE::UI::GetSingleton();
		if (registered || !ui) {
			return;
		}
		ui->RegisterSink<RE::MenuOpenCloseEvent>(new CloseSink());
		registered = true;
	}

	void AddColorTarget(RE::HUDMenu& a_menu, const Value& a_clip)
	{
		// Nothing would delete it as its menu goes.
		if (!g_deleteLink.Live()) {
			return;
		}
		auto target = std::make_unique<RE::BSGFxShaderFXTarget>(a_clip);
		target->SetToHUDColor(false);
		{
			const RE::BSAutoWriteLock l(a_menu.cachedQuadsLock);
			a_menu.shaderFXObjects.push_back(target.get());
		}

		const std::scoped_lock l(g_colorTargetsLock);
		g_colorTargets.push_back({ &a_menu, std::move(target) });
	}

	const bool* ComponentCanBeVisible(const RE::HUDMenu& a_menu, std::uintptr_t a_vtable)
	{
		// The first thing in any HUD part is its vtable, which says what kind
		// of part it is.
		for (const auto& component : a_menu.hudObjects) {
			if (component && *reinterpret_cast<const std::uintptr_t*>(component.get()) == a_vtable) {
				return &component->hudModes.canBeVisible;
			}
		}
		return nullptr;
	}

	void Waiter::Watch(Scaleform::GFx::Movie& a_movie, const char* a_what)
	{
		waiting = nullptr;
		if (on->GetValue()) {
			build(a_movie);
			return;
		}

		Value stage;
		if (!a_movie.GetVariable(&stage, "_root.stage") || !stage.IsObject()) {
			REX::WARN("The HUD has no stage to listen on, so {:s} stays off until the next save load.", a_what);
			return;
		}

		Value listener;
		a_movie.CreateFunction(&listener, this);
		if (!stage.Invoke("addEventListener", std::array{ Value("enterFrame"), listener })) {
			REX::WARN("The HUD refused a frame listener, so {:s} stays off until the next save load.", a_what);
			return;
		}
		waiting = &a_movie;
		TraceLog::Line("menu", "HUDMenu.swf loaded, {:s} waits for its switch", a_what);
	}

	void Waiter::Call(const Params& a_params)
	{
		if (!waiting || a_params.movie != waiting || !on->GetValue()) {
			return;
		}
		waiting = nullptr;
		build(*a_params.movie);
	}
}
