#pragma once

#include "Core/Plugin.h"

// Which pieces the game puts on as a race's or a character's skin, measured
// with the armor mods, see LoadMods in ArmorWear.h. Fallout 4 ships many
// skins without the flag that keeps the player from carrying them, so a
// piece that gives nothing and is a skin stays out, see WhyNoCondition.
// Private to this folder.
namespace ArmorWear
{
	// Reads the skin of every race and every character, templates too, into
	// a new set and swaps it in.
	void MeasureSkins();

	// Lets the set go, and no piece is a skin until MeasureSkins runs again.
	void UnloadSkins();

	// Whether a race or a character wears a_armor as its skin.
	bool IsSkin(const RE::TESObjectARMO& a_armor);
}
