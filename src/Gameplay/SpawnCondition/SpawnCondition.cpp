#include "Gameplay/SpawnCondition/SpawnCondition.h"

#include "Condition/Condition.h"
#include "Condition/Provenance/Provenance.h"
#include "Core/CallPatch.h"
#include "Core/TraceLog.h"
#include "Gameplay/SpawnCondition/Band.h"
#include "Gameplay/SpawnCondition/Guards.h"

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace SpawnCondition
{
	namespace
	{
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

		// What to call something in the trace log. A generic guard and every
		// creature's weapon are nameless on purpose, and an empty name would
		// leave a hole in the line. An editor ID would read better but costs
		// another engine call under the inventory lock.
		std::string_view NameOf(const RE::TESForm& a_form, std::string_view a_whenNameless)
		{
			const auto name = RE::TESFullName::GetFullName(a_form);
			return name.empty() ? a_whenNameless : name;
		}

		// Which halves were measured, for the trace log, so a weapon at a
		// surprising condition shows whether the list, the owner or neither
		// caused it.
		std::string Describe(const Provenance::Origin& a_origin)
		{
			std::string text;

			// The list by form ID, since a leveled list carries no name and the
			// ID finds it in the editor.
			if (a_origin.supply == Provenance::UNMEASURED) {
				text = "supply unknown";
			} else {
				text = std::format("supply {:.2f} [{:08X}]", a_origin.supply, a_origin.pipeline);
			}

			// 3 cases, not 2: a character with a rank, a character this could
			// not rank, and no character. Merging the middle into the last
			// would hide whether the scale is missing people.
			if (a_origin.care != Provenance::UNMEASURED && a_origin.keeper) {
				text += std::format(", care {:.2f} {:s} [{:08X}]", a_origin.care,
					NameOf(*a_origin.keeper, "unnamed character"), a_origin.keeper->formID);
			} else if (a_origin.keeper) {
				text += std::format(", care unmeasured for {:s} [{:08X}]",
					NameOf(*a_origin.keeper, "unnamed character"), a_origin.keeper->formID);
			} else {
				text += ", no owner";
			}

			return text;
		}

		// Whether a stack is going to the player. The player's own are
		// listed one by one and everybody else's counted, since a save
		// loading hands thousands to the characters and containers around the
		// player at once.
		[[nodiscard]] bool Players(const RE::BGSInventoryList* a_list)
		{
			return a_list && a_list->owner == RE::ObjectRefHandle{ RE::PlayerCharacter::GetPlayerHandle() };
		}

		// The record that makes the owner of a_list essential, a companion's
		// for example, or nothing. Read from the record, since the game marks
		// the character only once they are up and about, long after their gear
		// arrives. A leveled character is judged by the record placed in the
		// world, as the game reads it.
		[[nodiscard]] const RE::TESActorBase* Essential(const RE::BGSInventoryList* a_list)
		{
			if (!a_list) {
				return nullptr;
			}
			const auto owner = a_list->owner.get();
			if (!owner) {
				return nullptr;
			}

			const RE::TESActorBase* base = nullptr;
			if (owner->extraList) {
				if (const auto* leveled = owner->extraList->GetByType<RE::ExtraLeveledCreature>()) {
					base = leveled->originalBase;
				}
			}
			if (!base) {
				const auto* object = owner->GetObjectReference();
				if (object && object->Is(RE::ENUM_FORM_ID::kNPC_)) {
					base = static_cast<const RE::TESNPC*>(object);
				}
			}
			return base && base->IsEssential() ? base : nullptr;
		}

		// A stack for the trace log: a line of its own where it is the
		// player's or a trader's restock, and otherwise a count by what became
		// of it, with the line under the quiet loot tag for whoever needs it
		// back, see TraceLog.h.
		template <class... T>
		void Report(bool a_listed, std::string_view a_what, std::format_string<T...> a_fmt, T&&... a_args)
		{
			if (a_listed) {
				TraceLog::Line("spawn", a_fmt, std::forward<T>(a_args)...);
			} else {
				TraceLog::Count("spawn", a_what);
				TraceLog::Line("loot", a_fmt, std::forward<T>(a_args)...);
			}
		}

		// Whose stock an item is and the band it rolled in, for the trace log.
		std::string Stocked(const Restock& a_restock, const StockBand& a_band)
		{
			return std::format("stock of {:s}, who {:s}, rolled in {:.0f}% to {:.0f}%", a_restock.trader,
				a_band.repairs > 0 ? std::format("repairs it to {:d}%", a_band.repairs) : "does not repair it",
				a_band.low * 100.0F, a_band.high * 100.0F);
		}

		// Gives one stack a condition, if it is the kind of thing that has one
		// and has none yet. Returns what it aimed at, so the copies SplitOff
		// takes out roll the same way, or nothing when the stack was left
		// alone. Every item is logged to the trace log either way.
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

			// Where this item came from and who is about to hold it. A list
			// with no owner leaves both halves unmeasured.
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
			}
			return aim;
		}

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

		void AddStackHk(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack,
			std::uint32_t* a_oldCount, std::uint32_t* a_newCount)
		{
			// Before the original, on purpose. The original first looks for a
			// stack to merge the new one into, and stacks only merge when their
			// extra data matches. Rolling first keeps 2 pipe pistols from one
			// footlocker as 2 entries with their own health. Rolling afterwards
			// would merge them and give the pair one condition.
			const auto aim = Roll(a_list, a_object, a_stack);
			a_list->AddStack(a_object, a_stack, a_oldCount, a_newCount);

			// Splitting waits until the stack is in, since the split is the
			// engine's own and works on a stack inside an inventory.
			if (aim && a_stack->count > 1) {
				SplitOff(*a_list, *a_object, *a_stack, *aim);
			}
		}
	}

	void Install()
	{
		const auto patched = CallPatch::PatchAll(ADD_STACK_SITES, RE::ID::BGSInventoryList::AddStack,
			CallPatch::Repeat<std::size(ADD_STACK_SITES)>(reinterpret_cast<std::uintptr_t>(&AddStackHk)),
			"Weapons and armor spawn at a condition that suits where they came from");

		if (patched == std::size(ADD_STACK_SITES)) {
			// The limits a weapon can actually arrive at, not the clamp, which
			// is wider than anything Provenance asks for.
			const auto [floor, ceiling] = OrdinaryEnds();
			const auto lowest = std::max(floor, Provenance::LowestCentre() - SPREAD);
			const auto highest = std::min(ceiling, Provenance::HighestCentre() + SPREAD);

			REX::INFO("Weapons and armor spawn between {:.0f} and {:.0f} percent condition, worked out from the leveled list they came out of and who is carrying them. One in {:.0f} ignores that and lands anywhere between {:.0f} and {:.0f}.",
				lowest * 100.0F, highest * 100.0F, 1.0F / UPSET_CHANCE,
				std::max(Condition::MIN_HEALTH, WORST_SPAWN) * 100.0F, Condition::MAX_HEALTH * 100.0F);
		} else if (patched != 0) {
			REX::WARN("Some ways of spawning a weapon or a piece of armor will still hand it out at full condition.");
		}

		InstallGuards();
	}
}
