#pragma once

#include <cstdint>

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
}
