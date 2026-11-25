#pragma once

#include "Condition/ArmorWear/ArmorWear.h"
#include "Core/Plugin.h"

// How fast a piece of armor wears, against the ordinary weapon of the load
// order. Private to this folder.
namespace ArmorWear
{
	// The share of the copy whose list is a_extra.
	Share ShareOf(const RE::TESObjectARMO& a_armor, const RE::ExtraDataList* a_extra);

	// How much health one hit of a_damage costs this copy, given what it is
	// built from, which the piece and its extra data say. a_theirs for an
	// NPC's piece, at fNpcArmorWearRateMult with the line in the NPC log.
	// Armor mods that change the copy's Health change it, see Health.h. It is
	// ShareOf's value at that rate, for that hit.
	float Rate(const RE::TESObjectARMO& a_armor, const RE::ExtraDataList* a_extra, float a_damage, bool a_theirs);

	// How many blows of the ordinary weapon take a piece of ordinary make from
	// new to broken, at the player's rate as set, for the log. 0 when armor
	// never wears, at a rate of 0 or below.
	float BlowsToBreak();
}
