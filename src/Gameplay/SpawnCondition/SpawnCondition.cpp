#include "Gameplay/SpawnCondition/SpawnCondition.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/Condition.h"
#include "Condition/Provenance/Provenance.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/SpawnCondition/Band.h"
#include "Gameplay/SpawnCondition/Guards.h"
#include "Gameplay/SpawnCondition/Owners.h"
#include "Gameplay/SpawnCondition/Trace.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>

namespace SpawnCondition
{
	namespace
	{
		// -------------------------------------------------------------------
		// Where items enter an inventory
		// -------------------------------------------------------------------

		// The 6 calls to BGSInventoryList::AddStack, the one place a weapon can
		// enter an inventory. A raider's gun, a footlocker's contents, a
		// leveled list resolving, a quest reward, a console additem, a vendor
		// restocking and the player picking a rifle off a table all pass here.
		// 3 functions call it, twice each, once on a fast path with the write
		// lock already held and once on a slow path that takes it, and there is
		// no other way in.
		constexpr CallPatch::CallSite ADD_STACK_SITES[] = {
			{ RE::ID::BGSInventoryList::AddItem1.id(), 0x04C, "additem locked" },
			{ RE::ID::BGSInventoryList::AddItem1.id(), 0x071, "additem" },
			{ RE::ID::BGSInventoryList::AddItem2.id(), 0x055, "addcount locked" },
			{ RE::ID::BGSInventoryList::AddItem2.id(), 0x07B, "addcount" },
			{ 2194196, 0x12F, "container locked" },
			{ 2194196, 0x155, "container" },
		};

		// -------------------------------------------------------------------
		// The roll
		// -------------------------------------------------------------------

		// Gives one stack a condition, if it is the kind of thing that has one
		// and has none yet. Returns what it aimed at, so the copies SplitOff
		// takes out roll the same way, or nothing when the stack was left
		// alone. Every weapon and piece of armor is logged to the trace log
		// either way.
		std::optional<Aim> Roll(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack)
		{
			if (!a_object || !a_stack) {
				return std::nullopt;
			}

			// Anything that is not a weapon or armor returns without logging.
			// Every tin can and bottle cap comes through here.
			if (!a_object->IsWeapon() && !a_object->Is(RE::ENUM_FORM_ID::kARMO)) {
				return std::nullopt;
			}

			const auto name = NameOf(*a_object, "unnamed item");
			const auto id = a_object->formID;
			const auto count = a_stack->count;
			const auto players = Players(a_list);

			// A trader's chest restocking, whose stock rolls in the trader's
			// band, see Restock in SpawnCondition.h.
			const auto* restock = Restocking(a_list);
			const bool  listed = players || restock;

			// Items arrive in bursts, a whole area at once, so they share one
			// block in the trace log. A restock opens a block of its own with
			// the trader's name, see VendorRepair/Upkeep.cpp.
			if (!restock) {
				TraceLog::Group("SPAWN", "weapons and armor entering inventories");
			}

			if (const auto* why = Condition::WhyNoCondition(*a_object)) {
				Report(listed, why, "{:<30s} [{:08X}] x{:<3d} left alone, {:s}", name, id, count, why);
				return std::nullopt;
			}

			// Nowhere to write the health. A stack with no extra data list is
			// identical to every other copy of the form.
			if (!a_stack->extra) {
				Report(listed, "with no extradata", "{:<30s} [{:08X}] x{:<3d} left alone, no extradata to write to", name, id, count);
				return std::nullopt;
			}

			// A script handing the player a gift, a quest item or something a
			// character gives away, is a reward, so it arrives new even though
			// it was rolled in the giver's hands, like the gun Paladin Brandis
			// gives away. Only a stack nothing else shares, or the write would
			// reach the giver's copy too.
			if (players && GiftFromScript(*a_stack->extra) && Private(*a_stack)) {
				const auto before = a_stack->extra->GetHealthPerc();
				a_stack->extra->SetHealthPerc(Condition::MAX_HEALTH);
				Report(listed, "a gift", "{:<30s} [{:08X}] x{:<3d} starts at {:.4f}, a gift, was {:.4f}",
					name, id, count, Condition::MAX_HEALTH, before);
				return std::nullopt;
			}

			// -1 is a stack with no health extra data, which every item in an
			// unmodified game is until something writes to it. Once written it
			// is the item's own history, so a gun that changes hands keeps its
			// condition. At or above 0, not above: 0 is broken, and treating it
			// as no health would let a broken minigun repair itself by changing
			// hands.
			const auto existing = a_stack->extra->GetHealthPerc();
			if (existing >= 0.0F) {
				Report(listed, "already set", "{:<30s} [{:08X}] x{:<3d} left alone, already at {:.4f}", name, id, count, existing);
				return std::nullopt;
			}

			// Anything a console command hands over is the player giving
			// themselves something, so it arrives new. Written as a health,
			// since an item with none would be rolled the first time it changed
			// hands. Several at once stay one stack.
			if (FromConsole() && ConsoleTarget(a_list)) {
				a_stack->extra->SetHealthPerc(Condition::MAX_HEALTH);
				Report(listed, "given by the console", "{:<30s} [{:08X}] x{:<3d} starts at {:.4f}, from the console",
					name, id, count, Condition::MAX_HEALTH);
				return std::nullopt;
			}

			// A script handing the player something is a quest paying out, a
			// character giving a gift or another mod's script, so it arrives new
			// as well.
			if (FromScript() && players) {
				a_stack->extra->SetHealthPerc(Condition::MAX_HEALTH);
				Report(listed, "given by a script", "{:<30s} [{:08X}] x{:<3d} starts at {:.4f}, from a script",
					name, id, count, Condition::MAX_HEALTH);
				return std::nullopt;
			}

			// Several items in one stack are rolled only where SplitOff can
			// give each its own condition afterwards. A stack copied out of an
			// inventory that keeps it is left for the day the items really
			// change hands.
			if (count > 1 && !Private(*a_stack)) {
				Report(listed, "copied from an inventory that keeps it",
					"{:<30s} [{:08X}] x{:<3d} left alone, copied from an inventory that keeps it",
					name, id, count);
				return std::nullopt;
			}

			// A showpiece arrives new. A chest does not say which trader it
			// belongs to, so any chest counts, a reward chest's legendary too.
			if (Showpiece(a_list, *a_stack->extra)) {
				a_stack->extra->SetHealthPerc(Condition::MAX_HEALTH);
				Report(listed, "a showpiece", "{:<30s} [{:08X}] x{:<3d} starts at {:.4f}, legendary, in a chest",
					name, id, count, Condition::MAX_HEALTH);
				return std::nullopt;
			}

			// What the player's own record dresses them in arrives at full
			// condition, as an essential character's gear does. Loot they pick
			// up later still rolls.
			if (const auto* outfit = players ? PlayersOutfit(*a_stack->extra) : nullptr) {
				a_stack->extra->SetHealthPerc(Condition::MAX_HEALTH);
				Report(listed, "the player's own outfit", "{:<30s} [{:08X}] x{:<3d} starts at {:.4f}, outfit [{:08X}] of the player's own record",
					name, id, count, Condition::MAX_HEALTH, outfit->formID);
				return std::nullopt;
			}

			// An essential character's weapons and armor arrive new, and the
			// armor they are given never wears, see ArmorEvents.h.
			if (const auto* essential = Essential(a_list)) {
				a_stack->extra->SetHealthPerc(Condition::MAX_HEALTH);
				Report(listed, "for an essential character", "{:<30s} [{:08X}] x{:<3d} starts at {:.4f}, for {:s} [{:08X}], who is essential",
					name, id, count, Condition::MAX_HEALTH, NameOf(*essential, "unnamed character"), essential->formID);
				return std::nullopt;
			}

			// A trader's own stock rolls in the trader's band, so where it came
			// from is never asked.
			const auto stock = restock && restock->band ? std::optional{ restock->band(*a_object) } : std::nullopt;

			// Where this item came from and who is about to hold it. An owner
			// that is not a character, a footlocker for one, leaves the care
			// half unmeasured.
			const auto origin = a_list && !stock ? Provenance::Of(*a_list, *a_stack, Condition::KindOf(*a_object)) :
			                                       Provenance::Origin{};

			const Aim  aim{ origin.Centre(), stock };
			const auto rolled = RollHealth(aim);
			a_stack->extra->SetHealthPerc(rolled.health);

			// Spelled out only where a trace file takes it, since a save
			// loading hands thousands of stacks through here.
			if (TraceLog::IsOpen()) {
				Report(listed, "rolled", "{:<30s} [{:08X}] x{:<3d} starts at {:.4f}, {:s}{:s}",
					name, id, count, rolled.health, stock ? Stocked(*restock, *stock) : Describe(origin),
					rolled.upset ? ", ignored its band" : "");
				// The list the health went on, to match against the one the
				// Pip-Boy reads, see ItemCard/Hooks.cpp.
				if (listed) {
					const auto* legendary = a_stack->extra->GetLegendaryMod();
					TraceLog::Line("spawn", "  {:s} [{:08X}] rolled on extra data {:p}, {:s}", name, id,
						static_cast<const void*>(a_stack->extra.get()),
						legendary ? std::format("legendary {}", TraceLog::Who{ legendary }) : "no legendary");
				}
			}
			return aim;
		}

		// -------------------------------------------------------------------
		// Splitting a stack that arrived several at once
		// -------------------------------------------------------------------

		// Picks out the stack that just went in, by the extra data list it
		// shares with the one handed to AddStack, while it still holds more
		// than one item.
		class Arrived final :
			public RE::BGSInventoryItem::StackDataCompareFunctor
		{
		public:
			explicit Arrived(const RE::ExtraDataList* a_extra) noexcept :
				extra(a_extra)
			{}

			bool CompareData(const RE::BGSInventoryItem::Stack& a_stack) override
			{
				return a_stack.extra.get() == extra && a_stack.count > 1;
			}

		private:
			const RE::ExtraDataList* extra;
		};

		// Rolls a condition of its own for the copy the engine has just split
		// off, aimed where the stack was.
		class Reroll final :
			public RE::BGSInventoryItem::StackDataWriteFunctor
		{
		public:
			explicit Reroll(const Aim& a_aim) noexcept :
				aim(a_aim)
			{}

			void WriteDataImpl(RE::TESBoundObject&, RE::BGSInventoryItem::Stack& a_stack) override
			{
				// The copy starts with every flag the stack had, including the
				// ones marking the item in use, and 2 stacks marked that way
				// would confuse the game about which one is in use. The marks
				// stay with the stack that went in.
				a_stack.flags.reset(RE::BGSInventoryItem::Stack::Flag::kSlotMask,
					RE::BGSInventoryItem::Stack::Flag::kInvShouldEquip);

				if (a_stack.extra) {
					rolled = RollHealth(aim);
					a_stack.extra->SetHealthPerc(rolled.health);
				}
			}

			Aim    aim;
			Rolled rolled{};
		};

		// Gives every item in a stack that arrived several at once a
		// condition of its own. Roll gave the whole stack one condition between
		// them. 2 kinds still arrive like that: TESObjectREFR::AddInventoryItem
		// hands over an item without an object template all at once, and a
		// save made before this mod holds stacks the game merged. This splits
		// the stack the way a workbench repair takes 1 gun out of 6, and rolls
		// each copy. Runs straight after AddStack under the caller's write
		// lock.
		void SplitOff(RE::BGSInventoryList& a_list, RE::TESBoundObject& a_object, const RE::BGSInventoryItem::Stack& a_stack,
			const Aim& a_aim)
		{
			const auto name = NameOf(a_object, "unnamed item");
			const auto id = a_object.formID;
			const auto count = a_stack.count;

			RE::BGSInventoryItem* item = nullptr;
			for (auto& entry : a_list.data) {
				if (entry.object == &a_object) {
					item = &entry;
					break;
				}
			}

			// AddStack keeps a copy of the stack sharing its extra data list,
			// which is how Arrived finds it. A stack that merged into one
			// already there took that one's list and is not this call's to
			// split. AddStack told the Pip-Boy about the whole stack, so every
			// copy split off is announced the way the engine announces its own
			// writes, for the player's inventory alone. The merge that comes
			// with announcing keeps the copies apart, since stacks only merge
			// when their conditions match.
			const bool players = Players(&a_list);
			const bool listed = players || Restocking(&a_list);
			const auto owner = players ? a_list.owner : RE::ObjectRefHandle{};
			Arrived    arrived{ a_stack.extra.get() };
			for (auto left = count; left > 1; left--) {
				Reroll reroll{ a_aim };
				if (!item || !item->FindAndWriteStackData(arrived, reroll, false, owner)) {
					if (left == count) {
						Report(listed, "kept as one stack", "{:<30s} [{:08X}] x{:<3d} kept as one stack, it joined one already there",
							name, id, count);
					}
					return;
				}
				Report(listed, "split off", "{:<30s} [{:08X}] x1   split off at {:.4f}{:s}{:s}",
					name, id, reroll.rolled.health, reroll.rolled.upset ? ", ignored its band" : "",
					players ? ", the player's, told the Pip-Boy" : "");
			}
		}

		// -------------------------------------------------------------------
		// The AddStack hook, and armor put back to full
		// -------------------------------------------------------------------

		// A piece of armor NEC does not wear that still carries a health under
		// full, see ArmorWear::Settles, goes back to full as it enters any
		// inventory, a save's own stacks among them. A wedding ring worn to 0
		// then goes on again. Nothing is taken off, and power armor is never
		// touched. Only a stack nothing else shares, as for a gift. Returns
		// true when it wrote. It runs whatever Worn loot says, since a piece
		// NEC does not wear has no repair.
		bool Settle(const RE::BGSInventoryList* a_list, const RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack)
		{
			if (!a_object || !a_stack || !a_stack->extra || !a_object->Is(RE::ENUM_FORM_ID::kARMO) || !Private(*a_stack)) {
				return false;
			}

			// -1 is no health and counts as new already.
			const auto before = a_stack->extra->GetHealthPerc();
			if (before < Condition::MIN_HEALTH || before >= Condition::MAX_HEALTH ||
				!ArmorWear::Settles(static_cast<const RE::TESObjectARMO&>(*a_object))) {
				return false;
			}

			a_stack->extra->SetHealthPerc(Condition::MAX_HEALTH);
			const auto* restock = Restocking(a_list);
			if (!restock) {
				TraceLog::Group("SPAWN", "weapons and armor entering inventories");
			}
			Report(Players(a_list) || restock, "back to full", "{:<30s} [{:08X}] x{:<3d} back to full, NEC does not wear it, was {:.4f}",
				NameOf(*a_object, "unnamed item"), a_object->formID, a_stack->count, before);
			return true;
		}

		using AddStack_t = void (*)(RE::BGSInventoryList*, RE::TESBoundObject*, RE::BGSInventoryItem::Stack*, std::uint32_t*, std::uint32_t*);
		std::array<CallPatch::Link<AddStack_t>, std::size(ADD_STACK_SITES)> g_links;

		template <std::size_t I>
		void AddStackHk(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack,
			std::uint32_t* a_oldCount, std::uint32_t* a_newCount)
		{
			// Before the original, on purpose. The original first looks for a
			// stack to merge the new one into, and stacks only merge when their
			// extra data matches. Rolling first keeps 2 pipe pistols from one
			// footlocker as 2 entries with their own health. Rolling afterwards
			// would merge them and give the pair one condition. Settling comes
			// first for the same reason, and a stack put back to full is not
			// rolled, so the trace names it once.
			const bool settled = g_links[I].Live() && Settle(a_list, a_object, a_stack);
			const auto aim = !settled && Settings::bSpawnCondition.GetValue() && g_links[I].Live() ? Roll(a_list, a_object, a_stack) : std::nullopt;
			g_links[I](a_list, a_object, a_stack, a_oldCount, a_newCount);

			// Splitting waits until the stack is in, since the split is the
			// engine's own and works on a stack inside an inventory.
			if (aim && a_stack->count > 1) {
				SplitOff(*a_list, *a_object, *a_stack, *aim);
			}
		}
	}

	void Install()
	{
		const auto hooks = CallPatch::PerSite<std::size(ADD_STACK_SITES)>([]<std::size_t I>() { return &AddStackHk<I>; });
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

		InstallGuards();
	}
}
