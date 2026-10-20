#pragma once

#include "Condition/Condition.h"
#include "Core/Plugin.h"

#include <cstddef>
#include <cstdint>
#include <optional>

// Walking the leveled lists, and where each ranks against the rest. Private to
// this folder.
namespace Provenance
{
	// How deep a chain of lists inside lists is followed. Vanilla nests about 4
	// deep, so this guards against broken data and no real list reaches it.
	inline constexpr std::size_t DEEPEST = 64;

	// The result of measuring the leveled lists: how many were ranked for
	// weapons, how many for armor, and how many could not be worked out.
	struct SupplyCount
	{
		std::size_t ranked;
		std::size_t armorRanked;
		std::size_t spoiled;
	};

	// What 1 list can give out, over everything it reaches. Weapons and
	// armor in 1 walk, since the same lists feed both and the walk is the
	// only slow part.
	struct Haul
	{
		std::uint64_t weaponWorth{ 0 };
		std::uint64_t weapons{ 0 };

		// The armor the same way, by what it sells for, for the supply half
		// of a piece of armor spawning.
		std::uint64_t armorValue{ 0 };
		std::uint64_t armors{ 0 };

		// What 1 pick is worth in armor rating, an average and not a total,
		// since a list usually gives out 1 entry. See HaulOf for the
		// exception. The care half reads it.
		float armorWorth{ 0.0F };

		[[nodiscard]] bool operator==(const Haul&) const = default;
	};

	// What MeasureSupply finds.
	struct Supply
	{
		// Where each leveled list ranks against every other list that can give
		// out a weapon, and the same for armor. Keyed by form ID, which is what
		// the engine records on the item.
		std::unordered_map<RE::TESFormID, float> weapons;
		std::unordered_map<RE::TESFormID, float> armor;
		// What each list that could be worked out gives out, and the lists that
		// could not. The care half reads a list's armor here rather than
		// walking again, a character the engine creates mid game included, and
		// the next measure compares them to say what moved.
		std::unordered_map<const RE::TESLevItem*, Haul> hauls;
		std::unordered_set<const RE::TESLevItem*>       ruined;
	};

	// Walks every leveled list, ranks the ones that give out weapons against
	// each other and the ones that give out armor against each other, and keeps
	// what one pick from each is worth in armor for the care half.
	SupplyCount MeasureSupply(Supply& a_out);

	// Where a leveled list ranks against every other list that gives out an
	// item of a_kind, or nothing for a list that was not ranked for it.
	std::optional<float> SupplyOf(const Supply& a_supply, RE::TESFormID a_list, Condition::Kind a_kind);

	// What one thing someone is given is worth in armor. A piece of armor is
	// worth what its record says. A leveled list is worth what one pick from it
	// gives, worked out by the measure. Anything else is worth nothing here.
	// Returns nothing at all for a list that could not be worked out, so a
	// character using one stays unmeasured. Only reads, so the spawn hook can
	// call it on any thread.
	std::optional<float> ArmorFrom(const Supply& a_supply, const RE::TESForm& a_form);
}
