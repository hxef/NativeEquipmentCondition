#pragma once

#include "Core/Plugin.h"

// Setting the condition of the weapon in hand from the console, so a test needs
// no workbench. The game has no command that writes a condition, but its table
// still holds ShowRepairMenu, srm, left over from Fallout 3 with a command that
// does nothing, so it is reused:
//
//   srm        brings the weapon in hand back to full
//   srm 25     sets it to 25%
//
// Written the way the workbench writes it, to the copy in hand, so the HUD, the
// Pip-Boy and the price follow.
namespace ConsoleRepair
{
	// Hands ShowRepairMenu the repair.
	void Install();
}
