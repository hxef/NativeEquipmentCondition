#pragma once

#include "Core/Plugin.h"

#include "UI/Repair/Columns/Columns.h"
#include "UI/Repair/SelectedItem.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// What a repair adds to the numbers the item card prints, a weapon's damage or
// a piece's resistances by type, for the columns of the 3 repair boxes. Each
// gain is the card's own number at the condition the repair reaches less the
// card's own number now, each with its fraction dropped as the card drops it,
// so a gain the card would not show reads +0.
namespace Gains
{
	// 1 type the card prints for the item, with its number now and at each
	// condition asked, before the card drops the fraction, and whether it is
	// an armor piece's resistance.
	struct Column
	{
		std::uint32_t      type{ 0 };
		float              now{ 0.0F };
		std::vector<float> at;
		bool               armor{ false };
	};

	// What Read found, and whether bRepairBoxGains was off as it read, so the
	// trace follows Read even when the switch flips in between.
	struct Found
	{
		std::vector<Column> columns;
		bool                off{ false };
	};

	// The card's numbers for a_item now and at each of a_levels, whole
	// percents. 1 column per type the card would print at any of those levels
	// and has a word for, in the card's order. No columns for an item that is
	// neither a weapon nor a piece of armor, or one no longer found, and none
	// for every item while bRepairBoxGains is off.
	[[nodiscard]] Found Read(const SelectedItem::Item& a_item, const std::vector<std::uint32_t>& a_levels);

	// Adds a row to a_table: a_kept, then the cell of each column of a_found
	// for the level of a_row in its full words, "+2 Damage", and the same cells
	// in their short words, "+2 DMG", to its shortGains.
	void AddRow(Columns::Table& a_table, std::vector<std::string> a_kept, const Found& a_found, std::size_t a_row);

	// The 2 card numbers of every column for the level of a_row and the gain
	// shown, for the trace, each type by the card's number for it:
	// "type 1 21.60 to 24.30 +3, type 4 4.00 to 4.50 +0", or "no numbers", or
	// "gains off" when bRepairBoxGains was off as Read read.
	[[nodiscard]] std::string Spelled(const Found& a_found, std::size_t a_row);
}
