#pragma once

#include "Core/Plugin.h"

#include "UI/Repair/Columns/Columns.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// How wide the cells of a box's rows print, and the rows as html. Private to
// this folder.
namespace Columns
{
	// The font of a row of vanilla's message box, which a message box mod was
	// seen to keep: the main bold font at 18, with no letter spacing and no
	// kerning, as the box's own SetText leaves a row.
	inline constexpr const char* ROW_FONT = "$MAIN_Font_Bold";
	inline constexpr int         ROW_SIZE = 18;

	// The gain cells are drawn smaller, so more rows keep them in vanilla's
	// room. 14 is 21 px at 1080p. Only the size is set, so they keep the
	// colour the box gives its text.
	inline constexpr int GAIN_SIZE = 14;

	// The space after each column but the last, in the row's pixels, and
	// between the cells of a row drawn as 1 line.
	inline constexpr double           GAP = 8.0;
	inline constexpr std::string_view LINE_GAP{ "  " };

	// How wide a row's html prints in the row's font, measured by a text field
	// of NEC's own that is never on screen. Html that repeats is measured once.
	class Meter
	{
	public:
		explicit Meter(Scaleform::GFx::Movie& a_movie);

		// 0 for no html. Nothing when the field could not be made or html with
		// letters measures at nothing.
		[[nodiscard]] std::optional<double> Width(const std::string& a_html);

		// a_cell at a_size.
		[[nodiscard]] std::optional<double> Cell(const std::string& a_cell, int a_size);

	private:
		Scaleform::GFx::Value                   field;
		bool                                    ready{ false };
		std::unordered_map<std::string, double> known;
	};

	// How wide each cell of a_table prints, row by row, the columns before the
	// gains at a_size and the gains at GAIN_SIZE. Nothing when a cell could
	// not be measured.
	[[nodiscard]] std::optional<std::vector<std::vector<double>>> Measure(Meter& a_meter, const Table& a_table, int a_size);

	// Each column as wide as its widest cell.
	[[nodiscard]] std::vector<double> Widest(const std::vector<std::vector<double>>& a_widths);

	// The cells of a_shown as html, each column after the first behind a tab,
	// an empty cell too, so the next lands at its stop. A cell not at the
	// row's own size carries its size: a gain cell always, the others when
	// a_size is smaller.
	[[nodiscard]] std::string Tabbed(std::size_t a_kept, const std::vector<std::string>& a_cells, const std::vector<std::size_t>& a_shown, int a_size);

	// The cells of a_shown with letters, joined by LINE_GAP into 1 line of
	// html, each at its size as Tabbed gives it.
	[[nodiscard]] std::string Lined(std::size_t a_kept, const std::vector<std::string>& a_cells, const std::vector<std::size_t>& a_shown, int a_size);

	// Where each column of a_shown after the first starts, in whole pixels as
	// the row is sent, and how wide the row is then: its last stop and its
	// last column.
	struct Stops
	{
		std::vector<double> at;
		double              wide{ 0.0 };
	};

	[[nodiscard]] Stops StopsOf(const std::vector<double>& a_wide, const std::vector<std::size_t>& a_shown);
}
