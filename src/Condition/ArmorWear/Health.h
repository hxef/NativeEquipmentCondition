#pragma once

#include "Core/Plugin.h"

// How much longer a copy of a piece lasts for the armor mods on it that change
// its Health, measured with the armor mods, see LoadMods in ArmorWear.h.
// Private to this folder.
namespace ArmorWear
{
	// Finds the median Health of the load order's armor that has some, which
	// a piece with no Health of its own counts as. Says it in the log at the
	// first reading and again only when it moved.
	void MeasureHealth();

	// Forgets the median, and no mod changes any wear until MeasureHealth
	// runs again.
	void ForgetHealth();

	// How many times as long this copy lasts: its Health after its mods, as
	// the game works it out, over its Health without them. 1 for a copy with
	// no mod that changes Health, and never below 0.1. Read it under the
	// inventory lock that keeps a_extra.
	float HealthMult(const RE::TESObjectARMO& a_armor, const RE::ExtraDataList* a_extra);
}
