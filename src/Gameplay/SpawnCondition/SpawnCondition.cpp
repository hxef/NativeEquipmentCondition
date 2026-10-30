#include "Gameplay/SpawnCondition/SpawnCondition.h"

#include "Condition/Condition.h"
#include "Condition/Provenance/Provenance.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Runtime.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "Gameplay/SpawnCondition/Band.h"
#include "Gameplay/SpawnCondition/Fill.h"
#include "Gameplay/SpawnCondition/Food.h"
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
		//
		// OG's fill adds through AddItem2, which has AddStack inline, so
		// the 2 container sites have no call there and CONTAINER_FILL_SITE
		// does their work.
		constexpr std::size_t ADD_ITEM_SITES = 2;  // AddItem1's, first in the list

		constexpr CallPatch::CallSite ADD_STACK_SITES[] = {
			{ CallPatch::AE{ RE::ID::BGSInventoryList::AddItem1.ae(), 0x04C }, CallPatch::OG{ RE::ID::BGSInventoryList::AddItem1.og(), 0x04C }, "additem locked" },
			{ CallPatch::AE{ RE::ID::BGSInventoryList::AddItem1.ae(), 0x071 }, CallPatch::OG{ RE::ID::BGSInventoryList::AddItem1.og(), 0x071 }, "additem" },
			{ CallPatch::AE{ RE::ID::BGSInventoryList::BuildFromContainer_Impl.ae(), 0x12F }, CallPatch::NO_OG_COVERED, "container locked" },
			{ CallPatch::AE{ RE::ID::BGSInventoryList::BuildFromContainer_Impl.ae(), 0x155 }, CallPatch::NO_OG_COVERED, "container" },
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
			// load under the engine's loading flag, a form read later in play
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
		// Food spoiling's door
		// -------------------------------------------------------------------

		// Asks FoodSpoil whatever Worn loot says, naming the container a
		// handover takes the food out of. It leaves alone what the sort
		// leaves alone: a save being applied, a copy of another inventory's
		// stack, SpawnDupe's copies and the barter screen's.
		bool FoodArrives(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack)
		{
			if (!a_list || !a_object || !a_stack || !FoodSpoil::Spoils(*a_object)) {
				return false;
			}
			if (Loading() || !Private(*a_stack) || UnderSpawnDupe() || BarterCopy(a_list)) {
				return false;
			}
			return FoodSpoil::Arrive(*a_list, *a_object, *a_stack, HandedFrom(a_object, a_list));
		}

		// Whether a_stack is spoiled food copied into the list a cooking or
		// chemistry station counts from. The list then never gets it.
		bool LeftOut(const RE::BGSInventoryList* a_list, const RE::TESBoundObject* a_object, const RE::BGSInventoryItem::Stack* a_stack)
		{
			return a_list && a_object && a_stack && FoodSpoil::LeftOut(*a_list, *a_object, *a_stack);
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
		//
		// Food is stamped before the original too, so a merge compares the
		// stamp, and put in order once it is in. Found food rolls its starting
		// freshness first, so the stamp starts from it.
		template <std::size_t I>
		void AddItemHk(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack,
			std::uint32_t* a_oldCount, std::uint32_t* a_newCount)
		{
			const bool live = g_links[I].Live();
			if (live && LeftOut(a_list, a_object, a_stack)) {
				// AddStack zeroes both counts before anything else, so a stack
				// left out does the same.
				if (a_oldCount) {
					*a_oldCount = 0;
				}
				if (a_newCount) {
					*a_newCount = 0;
				}
				return;
			}
			if (live) {
				FoodFound(a_list, a_object, a_stack);
			}
			const bool food = live && FoodArrives(a_list, a_object, a_stack);
			const auto aim = Settings::bSpawnCondition.GetValue() && live ? Sort(a_list, a_object, a_stack) : std::nullopt;
			AddThenSplit<I>(aim, a_list, a_object, a_stack, a_oldCount, a_newCount);
			if (food) {
				FoodSpoil::Order(*a_list, *a_object);
			}
		}

		// A list filled from its base container's record. Only ever reached for
		// a list built or rebuilt from that record, never from saved stacks, so
		// each stack is born here and rolls. A load's rebuilds keep the vanilla
		// fill, and the save puts back what it holds. What rolls is counted
		// for FillHk, which records the stock.
		template <std::size_t I>
		void ContainerHk(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack,
			std::uint32_t* a_oldCount, std::uint32_t* a_newCount)
		{
			std::optional<Aim> aim;
			if (Settings::bSpawnCondition.GetValue() && g_links[I].Live()) {
				if (!Loading()) {
					// A negative count is left unrolled, as ContainerFillHk does.
					if (a_stack && static_cast<std::int32_t>(a_stack->count) >= 1) {
						aim = RollInFill(a_list, a_object, a_stack);
					}
				} else if (a_object && Gear(*a_object)) {
					CountApplied(a_list);
				}
			}
			AddThenSplit<I>(aim, a_list, a_object, a_stack, a_oldCount, a_newCount);
		}

		// OG's call to AddItem2 inside BuildFromContainer_Impl, where each
		// record line with mod slots joins the list. AddItem2 holds both
		// AddStack calls the fill has inline on 1.11.240.
		constexpr CallPatch::CallSite CONTAINER_FILL_SITE[] = {
			{ CallPatch::NO_AE, CallPatch::OG{ RE::ID::BGSInventoryList::BuildFromContainer_Impl.og(), 0x0FD }, "container fill" },
		};

		using AddItem2_t = void (*)(RE::BGSInventoryList*, RE::TESBoundObject*, std::uint32_t, RE::ExtraDataList*, std::uint16_t);
		std::array<CallPatch::Link<AddItem2_t>, std::size(CONTAINER_FILL_SITE)> g_containerFillLink;

		// ContainerHk for OG. The stack is built inside the call, so the
		// roll goes on the extra data list it is built from, which nothing else
		// holds yet. That is still before any merge, and the split comes once
		// the stack is in.
		void ContainerFillHk(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, std::uint32_t a_count, RE::ExtraDataList* a_extra,
			std::uint16_t a_flags)
		{
			std::optional<Aim> aim;
			if (Settings::bSpawnCondition.GetValue() && g_containerFillLink[0].Live()) {
				if (!Loading()) {
					// A negative count is kept as it is, which a split would
					// read as billions of copies, see RollPlainLines.
					if (static_cast<std::int32_t>(a_count) >= 1) {
						aim = RollInFill(a_list, a_object, a_extra, a_count, true);
					}
				} else if (a_object && Gear(*a_object)) {
					CountApplied(a_list);
				}
			}
			g_containerFillLink[0](a_list, a_object, a_count, a_extra, a_flags);
			if (aim && a_count > 1) {
				SplitOff(*a_list, *a_object, a_extra, a_count, *aim);
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
		if (Runtime::OldGen()) {
			const auto fillHooks = CallPatch::PerSite<std::size(CONTAINER_FILL_SITE)>([]<std::size_t I>() { return &ContainerFillHk; });
			if (CallPatch::PatchAll(CONTAINER_FILL_SITE, RE::ID::BGSInventoryList::AddItem2, fillHooks, g_containerFillLink,
					"Weapons and armor with mod slots that a container lists roll as each goes in") != std::size(CONTAINER_FILL_SITE)) {
				return;
			}
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
