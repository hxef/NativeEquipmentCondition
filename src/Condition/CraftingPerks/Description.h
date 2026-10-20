#pragma once

#include "Core/Plugin.h"

#include <span>

// Adds the repair discount line to each rank's perk description. Private to
// this folder.
namespace CraftingPerks
{
	struct Perks;  // Perks.h

	// Patches the 2 places a perk's own words are read for the screen.
	void InstallDescriptions();

	// Gives every rank of each perk in a_first its line, in a_perks.
	void TellRanks(Perks& a_perks, std::span<const RE::BGSPerk* const> a_first);
}
