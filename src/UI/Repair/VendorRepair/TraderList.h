#pragma once

#include "Core/Plugin.h"
#include "Core/Text/Text.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// The trader list, for a player or a patch that sets by hand how far a trader
// repairs a kind of gear. Private to this folder, but for Load and Unload,
// which Features.cpp runs. A file in Data/F4SE/Plugins/NEC/Traders/, such as
// MyTraders.ini, has 1 kind a line in the form Core/ListFiles.h gives, with
// the kind and how far after it:
//
//   Fallout4.esm|1679CE = armor 90
//
// The kinds are weapons, armor and clothing, and how far is 30 to 100 in steps
// of 10, none or measured. The ID names the trader as placed in the world,
// their NPC record or the faction they trade under, and a line of the first
// beats one of the second, which beats one of the third. measured is a level
// like any other, so a trader as placed set to measured keeps what their stock
// gives even when their faction has a line. NEC ships an example file there
// that changes nothing.
//
// Most traders as placed are not loaded when the list is read, since the game
// loads them with their cell, so a line whose form is not loaded is taken for a
// trader as placed, by its ID in the load order. A settler the game spawned
// lasts 1 save, so a line names their NPC record or faction instead.
namespace VendorRepair::TraderList
{
	// Reads every .ini file in the folder and says in NEC.log what each line
	// sets and why it skipped one. Keeps form IDs only, so Unload lets them go.
	// Read whatever Trader repairs says, so the switch turned on later finds
	// it read.
	void Load();

	// Forgets the list, so every trader is measured until Load runs.
	void Unload();

	// What 1 line sets, and where.
	struct Set
	{
		// The level, 0 for no repair, or nothing for measured.
		std::optional<std::uint32_t> level;
		std::string                  file;  // as NEC.log names it
		std::size_t                  line = 0;
	};

	// The line that sets a_trade for a_trader: theirs as placed, else their NPC
	// record's, else their vendor faction's, or nothing. Safe from any thread,
	// see Core/Snapshot.h.
	[[nodiscard]] std::optional<Set> Find(const RE::Actor& a_trader, Text::Trade a_trade);

	// The IDs a line names a_trader by, for the trace log, as
	// "Fallout4.esm|002CB9 as placed, Fallout4.esm|002CB7 as NPC,
	// Fallout4.esm|004AC7 as faction".
	[[nodiscard]] std::string Ids(const RE::Actor& a_trader);
}
