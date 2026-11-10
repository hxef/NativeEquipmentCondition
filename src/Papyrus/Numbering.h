#pragma once

#include "Core/Plugin.h"

#include "Papyrus/Items.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

// Numbering the stacks a script asks for, so each has a handle. A stack needs
// a number for a handle, see UniqueID.h, and NEC gives one only here, when a
// script asks for a handle. Private to this folder.
namespace Papyrus
{
	struct Target
	{
		RE::TESBoundObject* base{ nullptr };
		RE::ExtraDataList*  extra{ nullptr };
		std::uint16_t       uid{ 0 };

		// The persistent reference the stack holds, as a quest item's stack
		// does. Its handle is the reference itself, which the engine
		// numbers on its own and takes the number off again, so NEC never
		// numbers such a stack.
		RE::TESFormID ref{ 0 };
	};

	// Adds the stacks one function hands out to the list, in the
	// inventory's order, with the holder as an actor or null.
	using Pick = std::function<void(RE::Actor*, const RE::BGSInventoryList&, std::vector<Target>&)>;

	// What runs before HandlesFor lets go of the lock, which it tries once a
	// round and never waits for.
	struct Hold
	{
		// Run on each handle under the lock. False takes back a number
		// this call gave.
		const std::function<bool(const Item&)>* inside{ nullptr };

		// Set when a try failed.
		bool busy{ false };
	};

	// 1 handle per stack a_pick names in a_holder's inventory, in its
	// order. A stack with no number, or one whose number an earlier stack
	// holds, as a split leaves it, gets the lowest number free first. A
	// number is free when no stack holds it and the VM has no object for
	// it, and the VM may only be asked with no lock held. So each round
	// holds the lock, numbers what it can with the numbers already asked
	// about, and lets go to ask about more. Nothing while the inventory
	// stays busy.
	std::optional<std::vector<Item>> HandlesFor(RE::TESObjectREFR& a_holder, const Pick& a_pick, Hold& a_hold);
}
