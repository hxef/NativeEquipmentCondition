#pragma once

#include "Core/Plugin.h"

// The list in the box a workbench puts up to confirm a build, a scrap or a
// repair. The box lists every component the job takes, and its panel stops the
// list 400 below the top of the box, about 8 rows, with a down arrow under a
// longer list. The game scrolls it with the left stick alone, through
// onLeftThumbstickInput on the panel, and ignores every other key, so a repair
// that takes many kinds of component shows only its first rows on a keyboard.
//
// The box grows past the panel's limit as far as the screen allows, half as
// many rows again on 16:9, and takes the keys the game's own lists scroll with:
// the arrow keys and the D-pad, W and S while the menu under the box turns them
// into arrows, and the mouse wheel, each a row at a time, repeating at the pace
// of the game's lists. The Flash side is left alone, since the game sends the
// stick to the movie a second time as arrow keys and a listener there would
// scroll 2 rows a push.
namespace ConfirmScroll
{
	// Hooks the box.
	void Install();
}
