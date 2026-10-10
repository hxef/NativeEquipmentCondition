#pragma once

#include "Core/Plugin.h"

// Paying a trader to repair a worn weapon, piece of armor or piece of clothing.
// A workbench asks for components. A trader who deals in that kind of item asks
// for caps, and more of them, since the player pays for someone else's skill,
// so a player deep in Gun Nut repairs cheaply at a bench and one who never
// picked up a wrench pays a trader.
//
// The REPAIR button sits at the end of the barter screen's bar, beside INVEST,
// while the highlight is on a worn item on the player's side at a trader who
// repairs its kind, weapons, armor or clothing. Pressing it asks how far to
// repair the item, with each step's price on its button, and picking one is all
// it takes. Who deals in what, and how far they repair it, is read from what
// the trader restocks with, so a trader a mod adds lands on the same scale and
// buying the shelves bare changes nothing. Every step is priced from what the
// item owes, see Repair.h, so a trader who stops short of full costs nothing
// extra. A trader's own stock is in better shape the further they repair its
// kind, so the best shops sell nearly new.
//
// Restock.h works out what a trader restocks with, Stock.h reads the item under
// the highlight and what the trader deals in, Quote.h what the work costs,
// Button.h the button, Retry.h the wait for a bar under a UI replacer and
// the watch over it, Payment.h the paying and Upkeep.h the trader's own stock.
namespace VendorRepair
{
	// Patches the barter menu's function table, and the restock where
	// weapons and armor spawn worn.
	void Install();

	// Listens for the game's message boxes, which take REPAIR off the bar
	// while they are up.
	void Load();

	// Whether the barter highlight hook still runs, false once another mod
	// cut its place. A refresh no hook starts, for a message box or a frame
	// of a wait or a watch, asks it first, since it would show the item that
	// hook last stored.
	[[nodiscard]] bool HighlightLive();
}
