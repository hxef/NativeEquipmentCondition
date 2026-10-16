#include "Gameplay/SpawnCondition/SpawnCondition.h"

#include "Condition/Condition.h"
#include "Condition/Provenance/Provenance.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/SpawnCondition/Band.h"
#include "Gameplay/SpawnCondition/Guards.h"
#include "Gameplay/SpawnCondition/Handover.h"
#include "Gameplay/SpawnCondition/Owners.h"
#include "Gameplay/SpawnCondition/Roll.h"
#include "Gameplay/SpawnCondition/Trace.h"
#include "Gameplay/SpawnCondition/World.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace SpawnCondition
{
	namespace
	{
		// -------------------------------------------------------------------
		// Where items enter an inventory
		// -------------------------------------------------------------------

		// The 4 calls to BGSInventoryList::AddStack where a weapon is born into
		// an inventory or changes hands. A raider's gun, a footlocker's
		// contents, a leveled list resolving, a quest reward, a console
		// additem, a vendor restocking and the player picking a rifle off a
		// table all pass here. AddItem1 and BuildFromContainer_Impl call it
		// twice each, once on a fast path with the write lock already held and
		// once on a slow path that takes it. AddItem2 calls it too, but only for
		// invested caps and a list the examine menu builds for a moment, so it
		// is left alone.
		constexpr std::size_t ADD_ITEM_SITES = 2;  // AddItem1's, first in the list

		constexpr CallPatch::CallSite ADD_STACK_SITES[] = {
			{ RE::ID::BGSInventoryList::AddItem1.id(), 0x04C, "additem locked" },
			{ RE::ID::BGSInventoryList::AddItem1.id(), 0x071, "additem" },
			{ RE::ID::BGSInventoryList::BuildFromContainer_Impl.id(), 0x12F, "container locked" },
			{ RE::ID::BGSInventoryList::BuildFromContainer_Impl.id(), 0x155, "container" },
		};

		// -------------------------------------------------------------------
		// What the door tells the trace log
		// -------------------------------------------------------------------

		// A load hands thousands of stacks through the door, so they are only
		// counted, and only while a trace file is open.
		void CountApplied(const RE::BGSInventoryList* a_list)
		{
			if (TraceLog::IsOpen()) {
				OpenBlock(Restocking(a_list));
				TraceLog::Count("spawn", "while a save is applied");
			}
		}

		// A stack left as it is, counted by a_why, or listed where it is the
		// player's or a restock's, see Report. Works nothing out while no trace
		// file is open, since it runs under the inventory lock.
		void LeftAlone(const RE::BGSInventoryList* a_list, const RE::TESBoundObject& a_object,
			const RE::BGSInventoryItem::Stack& a_stack, std::string_view a_why)
		{
			if (!TraceLog::IsOpen()) {
				return;
			}
			const auto* restock = Restocking(a_list);
			OpenBlock(restock);
			Report(Players(a_list) || restock, a_why, "{:<30s} [{:08X}] x{:<3d} {:s}",
				NameOf(a_object, "unnamed item"), a_object.formID, a_stack.count, a_why);
		}

		// -------------------------------------------------------------------
		// The fill scope
		// -------------------------------------------------------------------

		// What the fill in progress on this thread has rolled, see FillHk, or
		// null outside a fill. A fill inside a fill counts for itself.
		thread_local std::uint32_t* t_filled = nullptr;

		// Roll, counted for the fill around it when it aims or writes a health.
		std::optional<Aim> RollInFill(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack)
		{
			if (!t_filled) {
				return Roll(a_list, a_object, a_stack);
			}
			const auto before = Condition::HealthOf(a_stack);
			auto       aim = Roll(a_list, a_object, a_stack);
			if (aim || Condition::HealthOf(a_stack) != before) {
				++*t_filled;
			}
			return aim;
		}

		// -------------------------------------------------------------------
		// AddItem1: the sort
		// -------------------------------------------------------------------

		// What a single stack added to an inventory gets, in this order. The
		// first test that matches decides. Returns the aim of a roll, so the
		// hook splits a stack of several after the add, or nothing.
		std::optional<Aim> Sort(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack)
		{
			if (!a_object || !a_stack) {
				return std::nullopt;
			}

			// Every tin can leaves here. A handover still looks at anything it
			// moves, since a world item rerolled into a form with no condition
			// can keep its mark.
			const auto* handing = HandingOver(a_object, a_list);
			if (!handing && !Gear(*a_object)) {
				return std::nullopt;
			}

			// Every stack a save holds arrives while it is applied: the whole
			// load under the game's loading flag, a form read later in play
			// under this thread's bit. It keeps what the save holds.
			if (Loading()) {
				CountApplied(a_list);
				return std::nullopt;
			}

			// A stack something else holds: a copy of another inventory's
			// stack, as a workbench gathers its linked containers or the barter
			// screen copies a trader's stock, and, as a net under Loading, a
			// stack a save chained into its item. A write would reach the other
			// holder.
			if (!Private(*a_stack)) {
				LeftAlone(a_list, *a_object, *a_stack, "a copy left alone");
				return std::nullopt;
			}

			if (UnderSpawnDupe()) {
				LeftAlone(a_list, *a_object, *a_stack, "left alone under SpawnDupe");
				return std::nullopt;
			}

			// The barter screen's copy of a trader's goods. Only a loose item
			// the trader owns rolls there, once for the item in the world.
			if (BarterCopy(a_list)) {
				if (!LooseItem(a_list, a_object, a_stack)) {
					LeftAlone(a_list, *a_object, *a_stack, "a trader's copy left alone");
				}
				return std::nullopt;
			}

			if (handing) {
				return HandOver(a_list, a_object, a_stack, *handing);
			}

			// Anything else is born here, so a road nobody listed still rolls.
			return Roll(a_list, a_object, a_stack);
		}

		// -------------------------------------------------------------------
		// The AddStack hooks
		// -------------------------------------------------------------------

		using AddStack_t = void (*)(RE::BGSInventoryList*, RE::TESBoundObject*, RE::BGSInventoryItem::Stack*, std::uint32_t*, std::uint32_t*);
		std::array<CallPatch::Link<AddStack_t>, std::size(ADD_STACK_SITES)> g_links;

		// Hands the stack on, then splits what a roll aimed at a stack of
		// several. Splitting waits until the stack is in, since the split is
		// the engine's own and works on a stack inside an inventory.
		template <std::size_t I>
		void AddThenSplit(const std::optional<Aim>& a_aim, RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object,
			RE::BGSInventoryItem::Stack* a_stack, std::uint32_t* a_oldCount, std::uint32_t* a_newCount)
		{
			g_links[I](a_list, a_object, a_stack, a_oldCount, a_newCount);
			if (a_aim && a_stack->count > 1) {
				SplitOff(*a_list, *a_object, *a_stack, *a_aim);
			}
		}

		// Before the original, on purpose. The original first looks for a stack
		// to merge the new one into, and stacks only merge when their extra
		// data matches. Rolling first keeps 2 pipe pistols from one footlocker
		// as 2 entries with their own health. Rolling afterwards would merge
		// them and give the pair one condition.
		template <std::size_t I>
		void AddItemHk(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack,
			std::uint32_t* a_oldCount, std::uint32_t* a_newCount)
		{
			const auto aim = Settings::bSpawnCondition.GetValue() && g_links[I].Live() ? Sort(a_list, a_object, a_stack) : std::nullopt;
			AddThenSplit<I>(aim, a_list, a_object, a_stack, a_oldCount, a_newCount);
		}

		// A list filled from its base container's record. Only ever reached for
		// a list built or rebuilt from that record, never from saved stacks, so
		// each stack is born here and rolls. A load's rebuilds keep the game's
		// own fill, and the save puts back what it holds. What rolls is counted
		// for FillHk, which records the stock.
		template <std::size_t I>
		void ContainerHk(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack,
			std::uint32_t* a_oldCount, std::uint32_t* a_newCount)
		{
			std::optional<Aim> aim;
			if (Settings::bSpawnCondition.GetValue() && g_links[I].Live()) {
				if (!Loading()) {
					aim = RollInFill(a_list, a_object, a_stack);
				} else if (a_object && Gear(*a_object)) {
					CountApplied(a_list);
				}
			}
			AddThenSplit<I>(aim, a_list, a_object, a_stack, a_oldCount, a_newCount);
		}

		// -------------------------------------------------------------------
		// Lists filled from a container's record
		// -------------------------------------------------------------------

		// The 2 calls to BGSInventoryList::BuildFromContainer_Impl: the list's
		// constructor, and a rebuild such as a container resetting. Both hold
		// the list's write lock. A line whose item has no mod slots, such as a
		// hat or a pair of glasses, never reaches AddStack. The fill writes it
		// straight into the list as 1 entry with the line's count.
		constexpr CallPatch::CallSite FILL_SITES[] = {
			{ RE::ID::BGSInventoryList::ctor.id(), 0x0A2, "list build" },
			{ RE::ID::BGSInventoryList::BuildFromContainer.id(), 0x02E, "list rebuild" },
		};

		using Fill_t = decltype(&RE::BGSInventoryList::BuildFromContainer_Impl);
		std::array<CallPatch::Link<Fill_t>, std::size(FILL_SITES)> g_fillLinks;
		CallPatch::Held                                            g_fill;

		// Rolls each entry the fill wrote from a_first on, as AddStack's hook
		// rolls the rest, and returns how many rolled. An entry whose item
		// went through AddStack already holds its roll and is skipped.
		std::uint32_t RollPlainLines(RE::BGSInventoryList& a_list, std::uint32_t a_first)
		{
			std::uint32_t rolled = 0;
			for (auto i = a_first; i < a_list.data.size(); i++) {
				auto&      entry = a_list.data[i];
				auto*      stack = entry.stackData.get();
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
			const auto    first = list->data.size();
			std::uint32_t rolled = 0;
			bool          result = false;
			{
				const Scoped fill{ t_filled, &rolled };
				result = g_fillLinks[I](a_container, a_list);
			}
			rolled += RollPlainLines(*list, first);
			RecordStock(*list, rolled);
			return result;
		}

		void InstallFill()
		{
			const CallPatch::Together fill{ Part::kModlessLoot };
			const auto                hooks = CallPatch::PerSite<std::size(FILL_SITES)>([]<std::size_t I>() { return &FillHk<I>; });
			CallPatch::PatchAll(FILL_SITES, RE::ID::BGSInventoryList::BuildFromContainer_Impl, hooks, g_fillLinks,
				"Weapons and armor with no mod slots that a container lists roll too, and the stock is kept");
			g_fill = fill.Set();
			if (!g_fill) {
				REX::WARN("Weapons and armor with no mod slots that a container lists arrive at full condition, and a container that always holds the same items can lose their conditions after a save and reload.");
			}
		}
	}

	void Install()
	{
		const auto hooks = CallPatch::PerSite<std::size(ADD_STACK_SITES)>([]<std::size_t I>() {
			if constexpr (I < ADD_ITEM_SITES) {
				return &AddItemHk<I>;
			} else {
				return &ContainerHk<I>;
			}
		});
		const auto patched = CallPatch::PatchAll(ADD_STACK_SITES, RE::ID::BGSInventoryList::AddStack, hooks, g_links,
			"Weapons and armor spawn at a condition that suits where they came from");

		if (patched != std::size(ADD_STACK_SITES)) {
			return;
		}

		// The limits a weapon can actually arrive at, not the clamp, which is
		// wider than anything Provenance asks for.
		const auto [floor, ceiling] = OrdinaryEnds();
		const auto lowest = std::max(floor, Provenance::LowestCentre() - SPREAD);
		const auto highest = std::min(ceiling, Provenance::HighestCentre() + SPREAD);

		REX::INFO("Weapons and armor spawn between {:.0f} and {:.0f} percent condition, worked out from the leveled list they came out of and who is carrying them. One in {:.0f} ignores that and lands anywhere between {:.0f} and {:.0f}.",
			lowest * 100.0F, highest * 100.0F, 1.0F / UPSET_CHANCE,
			std::max(Condition::MIN_HEALTH, WORST_SPAWN) * 100.0F, Condition::MAX_HEALTH * 100.0F);

		InstallFill();
		InstallHandover();
		InstallWorld();
		InstallGuards();
	}
}
