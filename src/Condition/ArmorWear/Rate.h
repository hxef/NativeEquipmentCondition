#pragma once

#include "Core/Plugin.h"

// How fast a piece of armor wears, against the ordinary weapon of the load
// order. Private to this folder.
namespace ArmorWear
{
	// How much health one hit of a_damage costs this copy, given what it is
	// built from, which the piece and its extra data say. a_theirs sends the
	// trace line to the NPC log.
	float Rate(const RE::TESObjectARMO& a_armor, const RE::ExtraDataList* a_extra, float a_damage, bool a_theirs);

	// How many blows of the ordinary weapon take a piece of ordinary make from
	// new to broken, at the rate as set, for the log. 0 when armor never
	// wears, at a rate of 0 or below.
	float BlowsToBreak();
}
