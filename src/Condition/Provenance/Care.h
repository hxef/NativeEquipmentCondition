#pragma once

#include "Condition/Provenance/Supply.h"
#include "Core/Plugin.h"

#include <cstddef>
#include <optional>
#include <vector>

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

	// What 1 character was found to be issued, kept for the next measure.
	struct Issued
	{
		// The outfit counted: the record's own, or for a character whose outfit
		// a script changed, the one the measure before counted.
		const RE::BGSOutfit* outfit{ nullptr };
		// The armor outfit and own inventory come to, or nothing where it could
		// not be worked out.
		std::optional<float> armor;

		[[nodiscard]] bool operator==(const Issued&) const = default;
	};

	// What MeasureCare finds.
	struct Care
	{
		// Where each character ranks against the rest by the armor they are
		// given. Keyed by base form, which the owner of an inventory points to
		// directly.
		std::unordered_map<const RE::TESNPC*, float> ranks;
		// Every ranked character's armor total, sorted, so a character who was
		// not there to be ranked can be placed among them.
		std::vector<float> ladder;
		// What each character not in power armor was issued.
		std::unordered_map<const RE::TESNPC*, Issued> issued;
	};

	// Ranks every character by the armor they are given. Reads what
	// MeasureSupply kept for each list, so it runs second. a_last is the
	// measure before, or nothing at Load.
	CareCount MeasureCare(const Supply& a_supply, const Care* a_last, Care& a_out);

	// Where one character ranks, 0 for the worst equipped to 1 for the best, or
	// UNMEASURED when nothing about them could be read. Safe on any thread.
	float CareOf(const Supply& a_supply, const Care& a_care, const RE::TESNPC& a_npc);
}
