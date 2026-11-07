#pragma once

#include "Core/Plugin.h"

#include "UI/Repair/Columns/Cells.h"
#include "UI/Repair/Columns/Columns.h"

#include <cstddef>
#include <optional>
#include <vector>

// Fitting a box's rows to the room by the steps Columns.h gives. Private to
// this folder.
namespace Columns
{
	// The smallest size the cells before the gains go down to, the gain
	// cells' own. Smaller letters are too hard to read, so a row still too
	// wide there has its name cut instead.
	inline constexpr int SMALLEST_SIZE = GAIN_SIZE;

	// A box's rows as they go out: the table in the words picked with any
	// names cut, the columns shown, the size of the cells before the gains
	// and each step taken. wide is the widest row as last measured, and
	// columns each column's width there for rows in columns.
	struct Fit
	{
		Table                    table;
		std::vector<std::size_t> shown;
		int                      size{ ROW_SIZE };
		bool                     shortWords{ false };
		std::size_t              cut{ 0 };
		std::optional<double>    leftOut;
		double                   wide{ 0.0 };
		std::vector<double>      columns;
	};

	// a_table fitted to a_room, from a_all's columns down to a_kept's, each
	// row in columns or, with a_lines, as 1 line. Nothing when a cell could
	// not be measured.
	[[nodiscard]] std::optional<Fit> Fitted(Meter& a_meter, const Table& a_table, const std::vector<std::size_t>& a_all,
		const std::vector<std::size_t>& a_kept, double a_room, bool a_lines);
}
