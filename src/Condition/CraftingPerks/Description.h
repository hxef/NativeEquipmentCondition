#pragma once

#include "Core/Plugin.h"

#include <span>

// Adds the repair discount line to each rank's perk description. Private to
// this folder.
namespace CraftingPerks
{
	// Patches the 2 places a perk's own words are read for the screen.
	void InstallDescriptions();

	// Gives every rank of each perk in a_first its line.
	void TellRanks(std::span<const RE::BGSPerk* const> a_first);

	// Forgets every rank TellRanks was given.
	void ForgetTold();
}
