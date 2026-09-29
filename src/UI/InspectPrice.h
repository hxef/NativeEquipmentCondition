#pragma once

#include "Core/Plugin.h"

// The price on the inspect screen at a trader. While a barter screen is open
// the game marks every price up or down for the trader, in whichever direction
// it priced last, and the inspect button sets none. So right after the screen
// opens the player's own items show what the trader would charge for them, and
// after a trade the trader's show what they would pay. Before the button does
// its work, the highlighted row is priced the way the quantity slider prices
// it, which points the markup at the item's owner: the trader's charge for
// theirs and their offer for the player's, a pending trade included.
// bInspectPrice in NEC.ini switches it off for a load order where another mod
// already fixes it.
namespace InspectPrice
{
	// Hooks the barter screen's calls from its movie.
	void Install();
}
