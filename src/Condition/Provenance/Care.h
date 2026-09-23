#pragma once

#include "Core/Plugin.h"

#include <cstddef>

// What a character is given, and where they rank against the rest. Private to
// this folder.
namespace Provenance
{
	// The result of measuring the characters: ranked in all, given armor, given
	// none, in power armor, and could not be worked out.
	struct CareCount
	{
		std::size_t ranked;
		std::size_t kits;
		std::size_t issuedNothing;
		std::size_t armored;
		std::size_t spoiled;
	};

	// Ranks every character by the armor they are given. Reads what
	// MeasureSupply kept for each list, so it runs second.
	CareCount MeasureCare();

	// Forgets everything MeasureCare kept.
	void ForgetCare();

	// Where one character ranks, 0 for the worst equipped to 1 for the best, or
	// UNMEASURED when nothing about them could be read. Safe on any thread once
	// Load has run.
	float CareOf(const RE::TESNPC& a_npc);
}
