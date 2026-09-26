#pragma once

#include "Core/Plugin.h"

// Fading the name of a worn out item in the Pip-Boy's lists, since the game
// refuses to equip one and the Pip-Boy gives no hint until it is tried. The
// game and the menu each build their own object per item, and only the item's
// number is shared, as nodeID. So MarkBroken writes down the numbers of worn
// out items as the game builds them, and the frame listener writes the name of
// any row carrying one in half white, which the Pip-Boy's tint turns into half
// strength Pip-Boy colour.
namespace Pipboy
{
	// Adds the frame listener to the Pip-Boy movie and ignores every other.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);

	// Notes whether the item this Pip-Boy entry stands for has worn out, by its
	// number, since a flag of our own would never reach the menu. ItemCard
	// calls this while the game builds the entry, worn out or not.
	void MarkBroken(const RE::PipboyObject& a_entry, bool a_broken);
}
