#pragma once

#include "Condition/Condition.h"
#include "Core/Plugin.h"

#include <cstdint>
#include <optional>
#include <vector>

// Reading the condition of what an actor holds and has on, and writing to one
// copy of an item. Every reader walks the actor's inventory for the equipped
// copies of the items that take part, under the inventory read lock.
//
// The Try readers take the lock as a writer instead, for a caller that is not
// running where the engine's own reader does. Other game threads split, merge
// and free stacks while holding only the read lock, so a walk could land on a
// stack as it is freed. They never wait for the lock: F4SE holds its queue
// while a task runs, so a thread holding the lock and queueing a task would
// wait on this one and the game would freeze. A failed try keeps the caller's
// last value.
//
// Identical items share one stack with one extra data list, so 6 pistols picked
// up together are one stack, and a condition written to it would belong to all
// 6. So the writers ask the engine to split the stack the way it does for a
// fusion core's charge: one item goes into a stack of its own with a copy of
// the list and the equipped flags, and the rest keep the condition they had.
// The 2 stay apart, since the engine only merges stacks whose lists match.
namespace Equipped
{
	// Any stack of the item: the all ones value the engine passes when it has
	// no particular stack in mind, which it reads as the equipped one where it
	// needs one.
	inline constexpr std::uint32_t ANY_STACK = static_cast<std::uint32_t>(-1);

	// The weapon the actor holds, or nothing when none takes part. Needed only
	// for the one bash path that names no weapon, see WeaponEvents/Hits.cpp.
	// WearsOut keeps grenades and mines out, so a rifle wins over the grenades
	// carried beside it.
	RE::TESObjectWEAP* Weapon(RE::Actor* a_actor);

	// The health of the equipped weapon, or INVALID_HEALTH when the actor holds
	// none that takes part. a_weapon limits the search to that form, so a
	// grenade, a mine or a shot fired before a weapon swap does not take the
	// condition of whatever is in hand when it lands. a_effect limits it to the
	// copy that casts that object effect, since the cast that applies a shot's
	// effect to the target names no weapon.
	float WeaponHealth(RE::Actor* a_actor, const RE::TESForm* a_weapon = nullptr,
		const RE::EnchantmentItem* a_effect = nullptr);

	// WeaponHealth for a caller outside the engine's own code path, a task on
	// F4SE's queue or an NPC's shot or hit. Nothing while the inventory is
	// busy.
	std::optional<float> TryWeaponHealth(RE::Actor* a_actor, const RE::TESForm* a_weapon = nullptr);

	// One piece of armor an actor has on that takes part.
	struct ArmorPiece
	{
		RE::TESObjectARMO* armor{ nullptr };

		// Its health, or INVALID_HEALTH for a piece nothing has written to,
		// which counts as new.
		float health{ Condition::INVALID_HEALTH };

		// The biped slots the piece fills, bit 0 for slot 30, the helmet, bit
		// 11 for the torso, 12 and 13 the arms, 14 and 15 the legs. Read from
		// the record, since a mod never moves a piece to another limb.
		std::uint32_t slots{ 0 };

		// Whether the piece is part of the wearer's own outfit from the editor,
		// see Issued in Equipped.cpp. A piece handed to them is not, even one
		// of the same kind.
		bool issued{ false };
	};

	// Every piece of armor the actor has on that takes part, in inventory
	// order. Empty for an actor with none, and for an actor in power armor,
	// whose pieces are the engine's.
	std::vector<ArmorPiece> ArmorPieces(RE::Actor* a_actor);

	// ArmorPieces for a caller outside the engine's own code path, see
	// TryWeaponHealth. Nothing while the inventory is busy.
	std::optional<std::vector<ArmorPiece>> TryArmorPieces(RE::Actor* a_actor);

	// Picks the equipped stack out of the ones the engine walks, for a writer.
	// A stack with no extra data list is skipped: the engine's split locks the
	// list it copies from without checking there is one, and would crash. Such
	// a stack has no health to write to anyway.
	class EquippedStack final :
		public RE::BGSInventoryItem::StackDataCompareFunctor
	{
	public:
		bool CompareData(const RE::BGSInventoryItem::Stack& a_stack) override;

		// How many items the stack held when picked, before the engine splits
		// it, and 0 while nothing was picked.
		std::uint32_t count{ 0 };
	};

	// Runs a_write on the one stack of a_object in a_owner's inventory that
	// a_find picks, and on one copy of it alone, see above. The engine finds
	// the entry, asks a_find which stack, splits off one copy with the equipped
	// flags when the stack holds more, hands the copy to a_write, merges what
	// became identical and tells the game the inventory changed, all under the
	// inventory lock, which is released when this returns.
	void WriteStack(RE::TESObjectREFR& a_owner, RE::TESBoundObject& a_object,
		RE::BGSInventoryItem::StackDataCompareFunctor& a_find, RE::BGSInventoryItem::StackDataWriteFunctor& a_write);

	// WriteStack on the equipped copy. Returns how many items its stack held, 0
	// when none of a_object was equipped.
	std::uint32_t WriteEquipped(RE::TESObjectREFR& a_owner, RE::TESBoundObject& a_object,
		RE::BGSInventoryItem::StackDataWriteFunctor& a_write);
}
