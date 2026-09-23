#pragma once

#include "Core/Plugin.h"

// Paying a trader to repair a worn weapon. A workbench asks for components. A
// trader who deals in weapons asks for caps, and more of them, since the player
// pays for someone else's skill, so a player deep in Gun Nut repairs cheaply at
// a bench and one who never picked up a wrench pays a trader.
//
// The REPAIR button sits at the end of the barter screen's bar, beside INVEST,
// while the highlight is on a worn weapon on the player's side at a trader who
// deals in weapons. Pressing it asks how far to repair the gun, with each
// step's price on its button, and picking one is all it takes. Who deals in
// weapons, and how far they repair a gun, is read from the shelves, so a trader
// a mod adds lands on the same scale. Every step is priced from what the weapon
// owes, see Repair.h, so a trader who stops short of full costs nothing extra.
//
// Stock.h reads the weapon under the highlight and the trader's shelves,
// Quote.h what the work costs, and Button.h the button and paying.
namespace VendorRepair
{
	// Patches the barter menu's function table.
	void Install();
}
