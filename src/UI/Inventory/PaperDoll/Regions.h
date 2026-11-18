#pragma once

#include "Core/Plugin.h"

#include <array>
#include <cstddef>
#include <cstdint>

// The 6 body regions of the Pip-Boy apparel figure, and the lowest condition
// among the pieces the player wears over each. Private to this folder.
namespace PaperDoll
{
	// The 6 regions, in the order the doll's own code names them.
	enum Region : std::size_t
	{
		kHead,
		kTorso,
		kLeftArm,
		kRightArm,
		kLeftLeg,
		kRightLeg,

		kRegions
	};

	// The entry over each region on the figure, in Region's order.
	inline constexpr std::array<const char*, kRegions> ENTRY_NAMES{
		"Head_Resist_mc",
		"Torso_Resist_mc",
		"LArm_Resist_mc",
		"RArm_Resist_mc",
		"LLeg_Resist_mc",
		"RLeg_Resist_mc",
	};

	// What a region shows when nothing that wears covers it, which hides
	// its bar.
	inline constexpr std::int32_t NONE = -1;

	// Asks for a fresh check, which arrives a frame or so later. Asking
	// again while one is on its way does nothing. The task takes the
	// inventory lock as a writer and never waits, so a busy inventory keeps
	// the last condition for a frame.
	void Queue();

	// The latest condition over a_region as a whole percent, or NONE, which
	// it always is while bNecOn is off.
	[[nodiscard]] std::int32_t LowestPercent(std::size_t a_region);

	// The same in tenths, as the piece's card writes it, see
	// Condition::CardTenths, or NONE whenever LowestPercent is.
	[[nodiscard]] std::int32_t LowestTenths(std::size_t a_region);
}
