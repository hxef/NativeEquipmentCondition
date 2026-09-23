#pragma once

#include "Core/Plugin.h"

// A CND bar on the HUD, in the ammo counter. The counter,
// RightMeters_mc.AmmoCount_mc in HUDMenu.swf, shows the rounds in the gun, the
// rounds in reserve and a divider line between them. The bar replaces the
// divider, a little thicker so its level is easy to read, shrinks from the left
// as the weapon wears, and the word CND follows past its end. The drawing is in
// HudParts.h, since the power armor readout is the same one at another size.
//
// The bar is a sprite beside the counter, not inside it. With a gun out it
// replaces the divider and fades with the counter. With a melee weapon out the
// game hides the counter, so the bar shows alone while the weapon is out. It
// hides when the HUD mode hides the counter, in the Pip-Boy for example, or in
// power armor, where PowerArmorCondition.h takes over. With no condition to
// show, the divider comes back and the HUD is exactly as it was.
//
// The bar is a HUD part of its own, with the native object the game gives each
// part, so the HUD tints it with its colour. It is updated from the HUD's own
// frames, which check the condition again every few frames, see
// HudParts::Weapon.
namespace HudCondition
{
	// Adds the bar to the HUD movie and ignores every other movie.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);
}
