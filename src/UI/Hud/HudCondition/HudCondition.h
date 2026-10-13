#pragma once

#include "Core/Plugin.h"

// A CND bar on the HUD, by the ammo counter. The counter,
// RightMeters_mc.AmmoCount_mc in HUDMenu.swf, shows the rounds in the gun, the
// rounds in reserve and a divider line between them. The bar replaces the
// divider, 3 times as thick so its level is easy to read, shrinks from the
// left as the weapon wears, and the word CND follows past its end. The drawing
// is in HudParts.h, since the power armor readout is the same one at another
// size.
//
// The bar is a sprite beside the counter, not inside it. With a gun out it
// replaces the divider and fades with the counter. With a melee weapon out the
// game hides the counter, so the bar shows alone while the weapon is out. It
// hides when the HUD mode hides the counter, in the Pip-Boy for example, or in
// power armor, where PowerArmorCondition.h takes over. With no condition to
// show, the divider comes back and the HUD is exactly as it was.
//
// A HUD mod can turn the divider, shorten it, or put a number or another part
// where the bar would go. Then the bar sits in the nearest free spot by the
// counter, and the divider shows. A word that would run off screen or cover a
// part moves beside the bar, see Spot.h. It is all measured from the movie a
// few times a second, with no check for any mod, so the bar follows a layout
// that changes. A counter turned as a whole turns the bar with it. fHudBarX
// and fHudBarY move the bar and its word from wherever they sit, and off the
// divider the divider shows again. The bar keeps its last spot while the
// divider is hidden or off screen.
//
// The bar is a HUD part of its own, with the native object the game gives each
// part, so the HUD tints it with its colour. It then takes the colour the
// divider is drawn in, so a HUD mod that colours the counter or the divider
// colours the bar too. It is updated from the HUD's own frames, which check
// the condition again every few frames, see HudParts::Weapon.
namespace HudCondition
{
	// Adds the bar to the HUD movie through HudParts::Waiter. Ignores every
	// other movie.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);
}
