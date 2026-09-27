#pragma once

#include "Core/Plugin.h"

// A trader's own stock is in about the condition they repair the player's gear
// to. Private to this folder.
//
// A trader's chests restock inside the call that makes the trader's side of the
// barter screen, as it opens or once a trade or an investment goes through, 2
// days on from the last restock. So what comes in is marked as that trader's
// stock, and each weapon or piece of armor rolls in a band that rises with how
// far the trader repairs its kind: 30% to 50% for a trader who repairs it to
// 30% or not at all, up to 85% to 100% for one who repairs it to full, see
// BandFor in Upkeep.cpp. 1 in 100 ignores its band, as loot does, and lands
// anywhere from 30% up. How far the trader repairs is worked out the way the
// REPAIR button works it out, as the first chest restocks.
//
// A plain line of a weapon or piece of armor with no object template goes into
// the chest without passing through AddStack, so it misses the upkeep and is
// rolled as ordinary loot later, if at all. A chest that restocks outside the
// barter screen, as its cell first loads or resets, rolls ordinary loot too,
// which the next due restock replaces.
namespace VendorRepair
{
	// Patches the calls that make the trader's side and restock their chests,
	// and says so in the log.
	void InstallUpkeep();
}
