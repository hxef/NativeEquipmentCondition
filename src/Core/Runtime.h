#pragma once

#include <string>

// The game version NEC runs on. NEC is made for 1.11.240 and runs on 1.10.163
// (OG) too. The side of a place's number comes from the fork's runtime
// check, see CallPatch/Site.h. Supported() also checks the version F4SE
// reports.
namespace Runtime
{
	// The game and F4SE versions F4SE reports, handed over as the plugin
	// loads.
	void Set(const REL::Version& a_game, const REL::Version& a_f4se);

	// Whether the game is 1.10.163 (OG).
	[[nodiscard]] bool OldGen();

	// Whether NEC knows the game: 1.11.240, the version every AE place
	// was read off, or 1.10.163. On any other version every pointer place
	// counts as taken, as what it holds can differ.
	[[nodiscard]] bool Supported();

	// The game version as text, such as 1.11.191.
	[[nodiscard]] std::string GameVersion();

	// NEC.log's line about the game and F4SE: "Game version 1.11.240 with
	// F4SE 0.7.9.", with "(OG)" after the version on 1.10.163. On a game
	// the runtime check does not know, it also says NEC stays off.
	[[nodiscard]] std::string Line();
}
