#pragma once

#include "Core/Plugin.h"

#include "Gameplay/SpawnCondition/Band.h"

#include <optional>

// A stack's first condition, and splitting a stack that arrived several at
// once. Private to this folder.
namespace SpawnCondition
{
	// Whether a_object is a weapon or a piece of armor, the only things that
	// roll.
	[[nodiscard]] bool Gear(const RE::TESBoundObject& a_object);

	// Gives one stack a condition, if it is the kind of thing that has one
	// and has none yet. Returns what it aimed at, so the copies SplitOff
	// takes out roll the same way, or nothing when the stack was left
	// alone. Every weapon and piece of armor is logged to the trace log
	// either way.
	std::optional<Aim> Roll(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack);

	// Gives every item in a stack that arrived several at once a
	// condition of its own. Roll gave the whole stack one condition between
	// them. A stack like that comes from a leveled list handing out several
	// of one item, a container's plain line with a count, or a world item
	// of several reaching its first holder. This splits the stack the way a
	// workbench repair takes 1 gun out of 6, and rolls each copy. Runs
	// straight after AddStack under the caller's write lock.
	void SplitOff(RE::BGSInventoryList& a_list, RE::TESBoundObject& a_object, const RE::BGSInventoryItem::Stack& a_stack,
		const Aim& a_aim);

	// The same for a stack whose entry the caller holds, as a fill that
	// writes entries straight into the list does. A search by item could
	// find another entry of the same item.
	void SplitOff(RE::BGSInventoryList& a_list, RE::BGSInventoryItem& a_entry, const RE::BGSInventoryItem::Stack& a_stack,
		const Aim& a_aim);
}
