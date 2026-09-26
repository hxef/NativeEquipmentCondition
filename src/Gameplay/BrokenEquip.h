#pragma once

#include "Core/Plugin.h"

// A broken item comes off like any other and stays off until it is repaired.
//
// The game refuses to equip an item at 0, "You must repair this item before
// equipping it.", see MIN_HEALTH in Condition.h. The Pip-Boy, a quick key and
// the container screen all equip and take off through a toggle that runs that
// check before it looks at whether the item is on, so a piece worn to 0 would
// be stuck on. That toggle also puts back what the game took off for the barber
// chair or the surgeon, and would leave a broken piece off after a new haircut.
// Install hooks the check: a broken item that is on may come off, one the game
// puts back goes back on, and one that is off stays off. The last holds in
// power armor too, where the game would let a broken hat on, and in a
// companion's trade screen, which would put on even a piece the game has
// refused. A quick key never takes a worn item off, so on a broken one it does
// nothing, as on any other.
namespace BrokenEquip
{
	void Install();
}
