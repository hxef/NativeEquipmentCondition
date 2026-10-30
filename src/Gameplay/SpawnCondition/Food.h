#pragma once

#include "Core/Plugin.h"

#include <cstdint>

// Found food's starting freshness, see FoodSpoil::SetStart, rolled where gear
// rolls: in a container's fill, as food is added to a box or a character,
// and as its first holder takes it. Only while Worn loot and food spoiling
// are both on. Private to this folder.
namespace SpawnCondition
{
	// For a_count of a_object born into a_list holding a_extra, in a
	// container's fill when a_fill: rolls the food's starting freshness, or
	// marks it fresh when a trader restocks it or a script or the console
	// gives it. Food the door adds to the player, a companion or a body, the
	// meat of a fresh kill too, is left to start fresh, and so is food a fill
	// gives the player or a companion. Returns whether it wrote.
	bool FoodBorn(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::ExtraDataList* a_extra, std::uint32_t a_count,
		bool a_fill);

	// For a_stack of a_object at the door into a_list, before FoodSpoil
	// stamps it: FoodBorn, or for a handover a roll for the food's first
	// holder. Food the player buys or a script hands the player starts fresh.
	void FoodFound(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack);
}
