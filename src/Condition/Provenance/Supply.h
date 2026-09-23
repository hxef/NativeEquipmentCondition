#pragma once

#include "Core/Plugin.h"

#include <cstddef>
#include <optional>

// Walking the leveled lists, and where each ranks against the rest. Private to
// this folder.
namespace Provenance
{
	// How deep a chain of lists inside lists is followed. Vanilla nests about 4
	// deep, so this guards against broken data and no real list reaches it.
	inline constexpr std::size_t DEEPEST = 64;

	// The result of measuring the leveled lists: how many were ranked, and how
	// many could not be worked out.
	struct SupplyCount
	{
		std::size_t ranked;
		std::size_t spoiled;
	};

	// Walks every leveled list, ranks the ones that give out weapons, and keeps
	// what one pick from each is worth in armor for the care half.
	SupplyCount MeasureSupply();

	// Forgets everything MeasureSupply kept.
	void ForgetSupply();

	// Where a leveled list ranks against every other list that gives out a
	// weapon, or nothing for a list that was not ranked.
	std::optional<float> SupplyOf(RE::TESFormID a_list);

	// What one thing someone is given is worth in armor. A piece of armor is
	// worth what its record says. A leveled list is worth what one pick from it
	// gives, worked out at Load. Anything else is worth nothing here. Returns
	// nothing at all for a list that could not be worked out, so a character
	// using one stays unmeasured. Only reads, so the spawn hook can call it on
	// any thread.
	std::optional<float> ArmorFrom(const RE::TESForm& a_form);
}
