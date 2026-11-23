#pragma once

#include "Core/Plugin.h"
#include "Core/Text/Text.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// The trader list, for a player or a patch that sets by hand how far a trader
// repairs a kind of gear. A file in Data/F4SE/Plugins/NEC/Traders/, such as
// MyTraders.ini, has lines in the form Core/ListFiles.h gives, each naming 1
// trader with 1 or more kinds after it, each kind with how far, split by
// commas:
//
//   Fallout4.esm|1679CE = armor 90
//   Fallout4.esm|002CB9 = weapons 100, armor none
//
// The kinds are weapons, armor and clothing, and how far is 30 to 100 in steps
// of 10, none or measured. Each kind of a line counts on its own, so a wrong
// one is skipped and the rest are kept. A kind set again for the same ID, in
// the same line or another, keeps the first. The ID is the trader's ref ID,
// their base ID or the faction they trade under, and a line of the first
// beats one of the second, which beats one of the third. measured is a level
// like any other, so a ref ID set to measured keeps what the trader's stock
// gives even when their faction has a line. NEC ships an example file there
// that changes nothing.
//
// Most traders' refs are not loaded when the list is read, since the game
// loads them with their cell, so a line whose form is not loaded is taken for
// a trader's ref ID, by its ID in the load order. A settler the game spawned
// lasts 1 save, so a line names their base ID or faction instead.
//
// Load and Unload are in VendorRepair.h, since Features.cpp runs them. Private
// to this folder.
namespace VendorRepair::TraderList
{
	// What 1 kind of a line sets, and where.
	struct Set
	{
		// The level, 0 for no repair, or nothing for measured.
		std::optional<std::uint32_t> level;
		std::string                  file;  // as NEC.log names it
		std::size_t                  line = 0;
	};

	// The line that sets a_trade for a_trader: their ref ID's, else their base
	// ID's, else their vendor faction's, or nothing. Safe from any thread, see
	// Core/Snapshot.h.
	[[nodiscard]] std::optional<Set> Find(const RE::Actor& a_trader, Text::Trade a_trade);

	// The IDs a line names a_trader by, for the trace log, as "ref ID
	// Fallout4.esm|002CB9, base ID Fallout4.esm|002CB7, faction
	// Fallout4.esm|004AC7".
	[[nodiscard]] std::string Ids(const RE::Actor& a_trader);
}
