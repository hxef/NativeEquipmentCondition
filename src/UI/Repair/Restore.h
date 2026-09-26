#pragma once

#include "Core/Plugin.h"

#include <cstdint>

// Writing a repair onto one copy of an item the player carries. The bench, the
// trader and the console all end here: the health is written onto the copy's
// extra data list the same way the power armor station repairs, through
// Equipped::WriteStack, and then a piece of armor is told to add its
// resistances up again, since it only does that as it goes on or comes off, see
// ArmorRating.h.
namespace Restore
{
	// Puts the stack of a_object that a_find picks at a_level percent. A menu
	// names the stack it shows by its number, the console the equipped one.
	void Write(RE::PlayerCharacter& a_player, RE::TESBoundObject& a_object,
		RE::BGSInventoryItem::StackDataCompareFunctor& a_find, std::uint32_t a_level);
}
