#include "Gameplay/SpawnCondition/Fill.h"

#include "Condition/Condition.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/SpawnCondition/Guards.h"
#include "Gameplay/SpawnCondition/Owners.h"
#include "Gameplay/SpawnCondition/Roll.h"
#include "Gameplay/SpawnCondition/Trace.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace SpawnCondition
{
	namespace
	{
		// -------------------------------------------------------------------
		// The fill scope
		// -------------------------------------------------------------------

		// What the fill in progress on this thread has rolled, see FillHk, or
		// null outside a fill. A fill inside a fill counts for itself.
		thread_local std::uint32_t* t_filled = nullptr;
	}

	std::optional<Aim> RollInFill(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::ExtraDataList* a_extra,
		std::uint32_t a_count, bool a_private)
	{
		if (!t_filled) {
			return Roll(a_list, a_object, a_extra, a_count, a_private);
		}
		const auto before = Condition::HealthOf(a_extra);
		auto aim = Roll(a_list, a_object, a_extra, a_count, a_private);
		if (aim || Condition::HealthOf(a_extra) != before) {
			++*t_filled;
		}
		return aim;
	}

	std::optional<Aim> RollInFill(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack)
	{
		return a_stack ? RollInFill(a_list, a_object, a_stack->extra.get(), a_stack->count, Private(*a_stack)) : std::nullopt;
	}

	namespace
	{
		// -------------------------------------------------------------------
		// Lists filled from a container's record
		// -------------------------------------------------------------------

		// The 2 calls to BGSInventoryList::BuildFromContainer_Impl: the list's
		// constructor, and a rebuild such as a container resetting. Both hold
		// the list's write lock. A line whose item has no mod slots, such as a
		// hat or a pair of glasses, never reaches AddStack. The fill writes it
		// straight into the list as 1 entry with the line's count.
		constexpr CallPatch::CallSite FILL_SITES[] = {
			{ CallPatch::AE{ RE::ID::BGSInventoryList::ctor.ae(), 0x0A2 }, CallPatch::OG{ RE::ID::BGSInventoryList::ctor.og(), 0x0AA }, "list build" },
			{ CallPatch::AE{ RE::ID::BGSInventoryList::BuildFromContainer.ae(), 0x02E }, CallPatch::OG{ RE::ID::BGSInventoryList::BuildFromContainer.og(), 0x02E }, "list rebuild" },
		};

		using Fill_t = decltype(&RE::BGSInventoryList::BuildFromContainer_Impl);
		std::array<CallPatch::Link<Fill_t>, std::size(FILL_SITES)> g_fillLinks;
		CallPatch::Held g_fill;

		// Rolls each entry the fill wrote from a_first on, as AddStack's hook
		// rolls the rest, and returns how many rolled. An entry whose item
		// went through AddStack already holds its roll and is skipped.
		std::uint32_t RollPlainLines(RE::BGSInventoryList& a_list, std::uint32_t a_first)
		{
			std::uint32_t rolled = 0;
			for (auto i = a_first; i < a_list.data.size(); i++) {
				auto& entry = a_list.data[i];
				auto* stack = entry.stackData.get();
				const auto health = Condition::HealthOf(stack);
				if (!entry.object || !stack || !stack->extra || !Gear(*entry.object) ||
					Condition::WhyNoCondition(*entry.object) || (health >= 0.0F && !Condition::IsUnrolled(health))) {
					continue;
				}
				// The fill keeps a negative count as it is, which a split would
				// read as billions of copies, so such a line is left alone.
				if (static_cast<std::int32_t>(stack->count) < 1) {
					continue;
				}
				const auto aim = Roll(&a_list, entry.object, stack);
				if (aim || Condition::HealthOf(stack) != health) {
					rolled++;
				}
				if (aim && stack->count > 1) {
					SplitOff(a_list, entry, *stack, *aim);
				}
			}
			return rolled;
		}

		// The record the engine makes for leveled lines, forced the same way so
		// a stock filled while nothing else records is kept. It does not stop a
		// later attach from remaking a stock nobody added to. The engine skips
		// it until the game first idles, and so does this. A temporary owner,
		// such as the barter screen's copy, is refused.
		void RecordStock(RE::BGSInventoryList& a_list, std::uint32_t a_rolled)
		{
			const auto* main = RE::Main::GetSingleton();
			if (a_rolled == 0 || !main || !main->onIdle) {
				return;
			}
			const auto owner = a_list.owner.get();
			if (!owner || !owner->AddChangeForced(RE::CHANGE_TYPES::kRefrLeveledInventory) || !TraceLog::IsOpen()) {
				return;
			}
			const auto* restock = Restocking(&a_list);
			OpenBlock(restock);
			Report(Players(&a_list) || restock, "a plain record line", "{:<30s} [{:08X}] its stock is recorded, {:d} rolled in its fill",
				NameOf(*owner, "unnamed owner"), owner->formID, a_rolled);
		}

		// Calls the fill with a scope up, so AddStack's hook counts what it
		// rolls, then rolls the plain lines and records the stock. Rolls
		// nothing while a save is applied.
		template <std::size_t I>
		bool FillHk(const RE::TESContainer* a_container, RE::BGSInventoryList** a_list)
		{
			auto* list = a_list ? *a_list : nullptr;
			if (!list || !Settings::bSpawnCondition.GetValue() || !g_fill.Runs(g_fillLinks[I]) || Loading()) {
				return g_fillLinks[I](a_container, a_list);
			}
			const auto first = list->data.size();
			std::uint32_t rolled = 0;
			bool result = false;
			{
				const Scoped fill{ t_filled, &rolled };
				result = g_fillLinks[I](a_container, a_list);
			}
			rolled += RollPlainLines(*list, first);
			RecordStock(*list, rolled);
			return result;
		}
	}

	void InstallFill()
	{
		const CallPatch::Together fill{ Part::kModlessLoot };
		const auto hooks = CallPatch::PerSite<std::size(FILL_SITES)>([]<std::size_t I>() { return &FillHk<I>; });
		CallPatch::PatchAll(FILL_SITES, RE::ID::BGSInventoryList::BuildFromContainer_Impl, hooks, g_fillLinks,
			"Weapons and armor with no mod slots that a container lists roll too, and the stock is kept");
		g_fill = fill.Set();
		if (!g_fill) {
			REX::WARN("Weapons and armor with no mod slots that a container lists arrive at full condition, and a container that always holds the same items can lose their conditions after a save and reload.");
		}
	}
}
