#pragma once

#include "Core/Plugin.h"

#include <string_view>

// A CND bar over each of the 6 resistance numbers on the Pip-Boy's paper doll,
// the figure the apparel tab draws with a number per body region. Each bar
// shows the lowest condition among the pieces the player wears over that
// region, head, torso, an arm or a leg, and stays hidden while nothing that
// wears covers it. A region counts the same slots the game counts for its
// number: the head is the helmet, goggles and gas mask, the torso and each limb
// their own armor slot plus the underarmor beneath, which the game counts for
// the torso and all 4 limbs.
//
// The doll is PaperDoll_mc on the inventory page, a movie of its own loaded
// into the Pip-Boy's, and each resistance is an entry with an icon and a
// number. The bar is a HudParts::Readout with no word, sized for the number the
// way every CND bar is sized for its text. It sits just above the icon, as wide
// as the icon and the number together, and is a child of the entry, so it moves
// and hides with it. The Pip-Boy tints white with its colour on its own, so the
// bar needs no colour target.
//
// The condition is checked through F4SE's task queue and kept in atomics, the
// way the HUD checks the weapon in hand, and a frame listener on the Pip-Boy
// movie draws it, since the doll sends no event when it changes.
namespace PaperDoll
{
	// Adds the frame listener to the Pip-Boy movie and ignores every other.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);
}
