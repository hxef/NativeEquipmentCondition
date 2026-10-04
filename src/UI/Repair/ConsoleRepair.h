#pragma once

#include "Core/Plugin.h"

// Setting the condition of what the player has equipped from the console, so a
// test needs no workbench. The game has no command that writes a condition, but
// its table still holds ShowRepairMenu, srm, left over from Fallout 3 with a
// command that does nothing, so it is reused:
//
//   srm            brings the weapon in hand back to full
//   srm 25         sets it to 25%
//   srm armor      brings every worn piece of armor that wears back to full
//   srm 25 armor   sets every one of them to 25%
//
// Written the way the workbench writes it, to the copy in hand or on the body,
// so the HUD, the Pip-Boy and the price follow.
namespace ConsoleRepair
{
	// Hands ShowRepairMenu the repair.
	void Install();

	// Puts back the help and parameters NEC found once a recheck cut srm's
	// place, so the console no longer shows NEC's help or expects NEC's
	// number and armor word for the mod that has srm now. Called after every
	// CallPatch::Recheck.
	void Settle();
}
