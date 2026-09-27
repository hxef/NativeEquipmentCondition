#pragma once

#include "Core/Plugin.h"

#include <unordered_map>

// What a trader restocks with, the stock a trader is judged by. Private to this
// folder.
//
// A trader restocks the first time their barter screen opens 2 days on from the
// last restock. Each chest behind them gets its stock list back and rolls its
// leveled lists again, and whatever the player sold them is gone. So the
// shelves are one roll among many, and they empty as the player buys. What is
// counted is every roll at once, worked out the way the game rolls: each item
// the stock lists can hand out, with its chance of turning up at all, 1 for a
// plain line or a sure pick, and the ammunition each gun brings. The same
// trader at the same level always comes out the same, however bare the shelves
// and however many guns the player sold. The lists, their chances and the
// chests are read as they stand, so a list a script adds to, a chance a
// caravan's upgrade lowers or a store raised a level counts at once.
//
// 3 of the game's rules are left out. A list with an epic loot chance can roll
// at a raised level, which in the base game changes no trader for a player of
// level 24 or more, and below it moves the repair limits at up to 11 of the 119
// vendor chests, every one up but Arturo's armor at level 10. A list that uses
// all can cap how many things it hands out, and the one stock list in the game
// that does sets the cap past anything it holds. A faction with a vendor
// location can have the containers it owns there take the place of all the
// rest, which changes no trader in the base game.
//
// The chests are the vendor faction's merchant container and the ones linked to
// the trader by a keyword of VendorKeywordLinkedRefFormList, which is how a
// settlement store's chests, 1 for every level up to its own, reach its
// settler. A chest rolls at its own level, its encounter zone's where it has
// one, and a list that picks one entry drops to the player's level when that
// is lower, as in the game.
//
// A chest a script fills has no stock list to read. Beside a chest that has
// one it is left out: it holds a unique piece until the player buys it, or it
// is the chest a settlement store is paid into, which gathers whatever the
// player sold every store of that kind. A trader with nothing else, Joe
// Savoldi at Bunker Hill in the base game, is counted as their shelves stand.
// The screen also shows the trader's own pockets, which are not stock and are
// left out.
namespace VendorRepair
{
	// Each item a trader restocks with and its chance of turning up. Only what
	// the barter screen would show is in it.
	using Chances = std::unordered_map<RE::TESBoundObject*, double>;

	// What a_trader restocks with. a_merchant is their vendor faction's
	// merchant container, and nothing or the trader for a faction with none.
	[[nodiscard]] Chances Restock(RE::TESObjectREFR* a_merchant, RE::Actor& a_trader);
}
