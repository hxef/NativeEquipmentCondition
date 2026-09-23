#pragma once

#include "Core/Plugin.h"

#include "Condition/CraftingPerks/CraftingPerks.h"
#include "Condition/Materials.h"
#include "UI/Repair/Workbench/Bench.h"

#include <cstdint>
#include <string>
#include <vector>

// What a repair costs at the weapon workbench. Private to this folder.
//
// A gun is a bill of components, its own and its mods', and repairing it from 0
// costs 2 times that bill with no rank of its perk, falling to 1 at the perk's
// top rank, see CraftingPerks.h. From any condition above 0 it costs less,
// along the curve in Repair.h. What is owed is a whole number of units from the
// bill, rounded once on the whole bill: rounding each component separately took
// a whole last 10 away on a gun built from many small amounts.
namespace Workbench
{
	// What repairing a weapon from 0 costs, as a multiple of everything it is
	// built from, before the player's perk is counted.
	inline constexpr float WRECK_MULTIPLE = CraftingPerks::UNSKILLED_MULTIPLE;

	// A multiple with fBenchCostMult on it, see Settings.h. Every multiple the
	// bench prices with passes through here, so the setting halves or doubles
	// every bill and each perk rank still takes off its share.
	[[nodiscard]] float Scaled(float a_multiple);

	// What a weapon is built from and which perk prices it, read once for every
	// level on offer. The order is the bill laid out one unit at a time, one
	// unit longer than the most the weapon could owe, so a repair is never
	// free.
	struct Priced
	{
		std::vector<Materials::Part>         built;
		std::vector<const RE::BGSComponent*> order;
		std::uint32_t                        units{ 0 };
		CraftingPerks::Standing              standing;
		float                                multiple{ WRECK_MULTIPLE };
	};

	[[nodiscard]] Priced PriceOf(const Selection& a_selection);

	// The components one repair takes, from one condition to another: the part
	// of the order between what is owed at each end, so a repair in steps adds
	// up to one in one go. Never empty: where both ends owe the same count the
	// next unit is charged.
	[[nodiscard]] std::vector<Materials::Part> CostOf(const Priced& a_priced, std::uint32_t a_from, std::uint32_t a_to);

	// The condition levels above the one the weapon already has.
	[[nodiscard]] std::vector<std::uint32_t> Above(const Selection& a_selection);

	// The levels worth offering: the ones above the current condition, minus
	// any costing exactly what the next level up costs. A board is 2 wood in
	// all, so every level from 60 up costs the same 1 unit.
	[[nodiscard]] std::vector<std::uint32_t> Offered(const Selection& a_selection);

	// What the trace file calls a bill of components.
	[[nodiscard]] std::string Spell(const std::vector<Materials::Part>& a_parts);

	// How many component units count for each perk, for the trace: one entry
	// per perk, most first. Parts whose recipe needs no perk are listed under
	// nobody.
	[[nodiscard]] std::string Vote(const Selection& a_selection);
}
