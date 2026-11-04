#pragma once

#include "Condition/Materials/Materials.h"

#include <cstdint>
#include <vector>

// What a worn copy scraps into. The game counts each component of a scrap
// from the count its recipes add up to, see Gameplay/Scrapping. NEC scales
// that count by the copy's condition first, and the game then halves it,
// applies its perks and Scrapper and rounds down as it always does. A count
// of 0 gives nothing.
namespace Materials
{
	// a_count scaled by a_percent, the copy's whole percent of condition from
	// 0 to 100, with a half rounding up: 11 Steel at 30% is 3, 2 Leather at
	// 75% is 2 and at 74% is 1. At 100 it is a_count, so new gear scraps as
	// vanilla.
	[[nodiscard]] std::uint32_t ScrapIn(std::uint32_t a_count, std::uint32_t a_percent);

	// What a copy of a_object with a_extra at a_percent is worth as a spare,
	// component by component: its scrap as the bench works it out, without
	// the halving. The bench's walk is the recipe of every enabled mod, the
	// legendary one too, and the item's own scrap recipe, with nothing
	// borrowed. Each form those recipes name is counted apart, scaled by
	// ScrapIn and then doubled, and the game's own count takes it from
	// there, so a Hunting Rifle's 2 Steel scrap items and the Steel its mods
	// ask for are 2 counts. A component the game counts at 0 is left out, so
	// without Scrapper a spare holds only the common ones. a_tenths scales
	// the doubled count before the game's count, in tenths and rounded down,
	// 10 for none and 5 for exactly what scrapping gives. a_extra can be
	// null.
	[[nodiscard]] std::vector<Part> Yield(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra,
		std::uint32_t a_percent, std::uint32_t a_tenths);
}
