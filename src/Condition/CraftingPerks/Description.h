#pragma once

#include "Core/Plugin.h"

#include <span>

// Adds NEC's lines to each rank's perk description, and keeps what the
// Pip-Boy perks page was given. Private to this folder.
namespace CraftingPerks
{
	struct Perks;  // Perks.h

	// Patches the 2 places a perk's own words are read for the screen.
	void InstallDescriptions();

	// Forgets what the Pip-Boy perks page was given, at Unload.
	void ForgetPipboy();

	// Gives every rank of each perk in a_first its line, in a_perks.
	void TellRanks(Perks& a_perks, std::span<const RE::BGSPerk* const> a_first);

	// Gives every rank of Scrapper the spare repair line, in a_perks. Its
	// ranks set the shares of the uncommon and rare components scrapping
	// gives, which is what a spare is worth, see Materials::Yield.
	void TellScrapper(Perks& a_perks);
}
