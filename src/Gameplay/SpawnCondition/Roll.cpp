#include "Gameplay/SpawnCondition/Roll.h"

#include "Condition/Condition.h"
#include "Condition/Provenance/Provenance.h"
#include "Core/TraceLog.h"
#include "Gameplay/SpawnCondition/Guards.h"
#include "Gameplay/SpawnCondition/Owners.h"
#include "Gameplay/SpawnCondition/Trace.h"

#include <format>
#include <optional>

namespace SpawnCondition
{
	// -------------------------------------------------------------------
	// The roll
	// -------------------------------------------------------------------

	bool Gear(const RE::TESBoundObject& a_object)
	{
		return a_object.IsWeapon() || a_object.Is(RE::ENUM_FORM_ID::kARMO);
	}

	std::optional<Aim> Roll(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack)
	{
		if (!a_object || !a_stack) {
			return std::nullopt;
		}

		// Anything that is not a weapon or armor returns without logging.
		// Every tin can and bottle cap comes through here.
		if (!Gear(*a_object)) {
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

		OpenBlock(restock);

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
		// unmodified game is until something writes to it. UNROLLED, a world
		// item no holder has rolled yet, counts as none too. Once written it
		// is the item's own history, so a gun that changes hands keeps its
		// condition. At or above 0, not above: 0 is broken, and treating it
		// as no health would let a broken minigun repair itself by changing
		// hands.
		const auto existing = a_stack->extra->GetHealthPerc();
		if (existing >= 0.0F && !Condition::IsUnrolled(existing)) {
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
		// character giving a gift or another mod's script, so it arrives
		// new as well.
		if (FromScript() && players) {
			a_stack->extra->SetHealthPerc(Condition::MAX_HEALTH);
			Report(listed, "given by a script", "{:<30s} [{:08X}] x{:<3d} starts at {:.4f}, from a script",
				name, id, count, Condition::MAX_HEALTH);
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

		// Spelled out only where a trace file takes it, since a cell
		// loading hands dozens of stacks through here.
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

	namespace
	{
		// Picks out the stack that just went in, by its extra data list,
		// while it still holds more than one item.
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

		// Splits a_stack inside a_item, 1 copy at a time, or logs it as kept
		// whole when there is no item.
		void Split(RE::BGSInventoryList& a_list, const RE::TESBoundObject& a_object, RE::BGSInventoryItem* a_item,
			const RE::BGSInventoryItem::Stack& a_stack, const Aim& a_aim)
		{
			const auto name = NameOf(a_object, "unnamed item");
			const auto id = a_object.formID;
			const auto count = a_stack.count;

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
				if (!a_item || !a_item->FindAndWriteStackData(arrived, reroll, false, owner)) {
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
	}

	void SplitOff(RE::BGSInventoryList& a_list, RE::TESBoundObject& a_object, const RE::BGSInventoryItem::Stack& a_stack,
		const Aim& a_aim)
	{
		RE::BGSInventoryItem* item = nullptr;
		for (auto& entry : a_list.data) {
			if (entry.object == &a_object) {
				item = &entry;
				break;
			}
		}
		Split(a_list, a_object, item, a_stack, a_aim);
	}

	void SplitOff(RE::BGSInventoryList& a_list, RE::BGSInventoryItem& a_entry, const RE::BGSInventoryItem::Stack& a_stack,
		const Aim& a_aim)
	{
		if (a_entry.object) {
			Split(a_list, *a_entry.object, &a_entry, a_stack, a_aim);
		}
	}
}
