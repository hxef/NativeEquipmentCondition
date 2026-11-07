#include "UI/Repair/Columns/Fit.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace Columns
{
	namespace
	{
		// -------------------------------------------------------------------
		// Measuring the rows and their short words
		// -------------------------------------------------------------------

		// How wide a_row prints as 1 line of a_fit's columns at its size.
		[[nodiscard]] std::optional<double> LineWidth(Meter& a_meter, const Fit& a_fit, const std::vector<std::string>& a_row)
		{
			return a_meter.Width(Lined(a_fit.table.kept, a_row, a_fit.shown, a_fit.size));
		}

		// Measures a_fit's rows, drawn as 1 line each when a_lines is set.
		// False when a cell could not be measured.
		[[nodiscard]] bool Measured(Meter& a_meter, Fit& a_fit, bool a_lines)
		{
			if (!a_lines) {
				const auto widths = Measure(a_meter, a_fit.table, a_fit.size);
				if (!widths) {
					return false;
				}
				a_fit.columns = Widest(*widths);
				a_fit.wide = StopsOf(a_fit.columns, a_fit.shown).wide;
				return true;
			}
			a_fit.wide = 0.0;
			for (const auto& row : a_fit.table.rows) {
				const auto width = LineWidth(a_meter, a_fit, row);
				if (!width) {
					return false;
				}
				a_fit.wide = std::max(a_fit.wide, *width);
			}
			return true;
		}

		// Puts each row's gains in their short words. False when no word
		// differs from its full one.
		[[nodiscard]] bool Shortened(Table& a_table)
		{
			auto changed = false;
			for (std::size_t i = 0; i < a_table.rows.size() && i < a_table.shortGains.size(); i++) {
				auto& row = a_table.rows[i];
				for (std::size_t gain = 0; gain < a_table.shortGains[i].size() && a_table.kept + gain < row.size(); gain++) {
					auto& cell = row[a_table.kept + gain];
					changed = changed || cell != a_table.shortGains[i][gain];
					cell = a_table.shortGains[i][gain];
				}
			}
			return changed;
		}

		// -------------------------------------------------------------------
		// Cutting the names
		// -------------------------------------------------------------------

		// What ends a name cut to fit, and the fewest letters a cut name
		// keeps, the spaces between them not counted.
		constexpr std::string_view CUT_MARK{ "..." };
		constexpr std::size_t      CUT_LETTERS = 8;

		// Cuts each name of a_table wider than the room a_rooms gives its row
		// to the most letters that fit with CUT_MARK after them, never fewer
		// than CUT_LETTERS, a space before the mark left off. A letter of 2 to
		// 4 bytes is never split. How many names were cut, or nothing when a
		// name could not be measured.
		[[nodiscard]] std::optional<std::size_t> Cut(Meter& a_meter, Table& a_table, const std::vector<double>& a_rooms, int a_size)
		{
			std::size_t cut = 0;
			for (std::size_t i = 0; i < a_table.rows.size(); i++) {
				if (*a_table.name >= a_table.rows[i].size()) {
					continue;
				}
				auto&      name = a_table.rows[i][*a_table.name];
				const auto width = a_meter.Cell(name, a_size);
				if (!width) {
					return std::nullopt;
				}
				if (*width <= a_rooms[i]) {
					continue;
				}

				// Where each letter starts, and how many a cut keeps at least.
				std::vector<std::size_t> starts;
				std::size_t              fewest = 0;
				std::size_t              letters = 0;
				for (std::size_t at = 0; at < name.size(); at++) {
					if ((static_cast<unsigned char>(name[at]) & 0xC0) != 0x80) {
						starts.push_back(at);
						if (name[at] != ' ' && ++letters == CUT_LETTERS) {
							fewest = starts.size();
						}
					}
				}
				if (fewest == 0 || fewest >= starts.size()) {
					continue;
				}
				const auto cutTo = [&](std::size_t a_letters) {
					auto out = name.substr(0, starts[a_letters]);
					out.erase(out.find_last_not_of(' ') + 1);
					return out + std::string{ CUT_MARK };
				};

				// Fewer letters never print wider, so the count kept is found by
				// halves.
				auto kept = fewest;
				auto tooMany = starts.size();
				while (tooMany - kept > 1) {
					const auto mid = (kept + tooMany) / 2;
					const auto at = a_meter.Cell(cutTo(mid), a_size);
					if (!at) {
						return std::nullopt;
					}
					(*at <= a_rooms[i] ? kept : tooMany) = mid;
				}
				const auto cutWidth = a_meter.Cell(cutTo(kept), a_size);
				if (!cutWidth) {
					return std::nullopt;
				}
				if (*cutWidth < *width) {
					name = cutTo(kept);
					cut++;
				}
			}
			return cut;
		}

		// Cuts a_fit's names to the room the rest of each row leaves them, in
		// whole pixels, and measures the rows again. In columns that room is
		// the same for every row, the other columns at their rounded stops, so
		// the stops after the name still hold. False when a cell could not be
		// measured.
		[[nodiscard]] bool CutNames(Meter& a_meter, Fit& a_fit, double a_room, bool a_lines)
		{
			const auto          name = *a_fit.table.name;
			std::vector<double> rooms;
			if (a_lines) {
				for (const auto& row : a_fit.table.rows) {
					const auto line = LineWidth(a_meter, a_fit, row);
					const auto cell = name < row.size() ? a_meter.Cell(row[name], a_fit.size) : std::optional{ 0.0 };
					if (!line || !cell) {
						return false;
					}
					rooms.push_back(std::floor(a_room - (*line - *cell)));
				}
			} else {
				auto others = a_fit.columns;
				others[name] = 0.0;
				rooms.assign(a_fit.table.rows.size(), std::floor(a_room - StopsOf(others, a_fit.shown).wide));
			}
			const auto cut = Cut(a_meter, a_fit.table, rooms, a_fit.size);
			a_fit.cut = cut.value_or(0);
			return cut && Measured(a_meter, a_fit, a_lines);
		}
	}

	std::optional<Fit> Fitted(Meter& a_meter, const Table& a_table, const std::vector<std::size_t>& a_all,
		const std::vector<std::size_t>& a_kept, double a_room, bool a_lines)
	{
		Fit fit;
		fit.table = a_table;
		fit.shown = a_all;
		auto measured = Measured(a_meter, fit, a_lines);
		if (measured && fit.wide > a_room && Shortened(fit.table)) {
			fit.shortWords = true;
			measured = Measured(a_meter, fit, a_lines);
		}
		while (measured && fit.wide > a_room && fit.size > SMALLEST_SIZE) {
			fit.size--;
			measured = Measured(a_meter, fit, a_lines);
		}
		const auto cuts = a_table.name && std::ranges::contains(a_kept, *a_table.name);
		if (measured && fit.wide > a_room && cuts) {
			measured = CutNames(a_meter, fit, a_room, a_lines);
		}

		// The last resort, for a box narrower than vanilla's. The names are cut
		// again from whole for the room the gains leave.
		if (measured && fit.wide > a_room && a_kept.size() < a_all.size()) {
			fit.leftOut = fit.wide;
			fit.shown = a_kept;
			fit.cut = 0;
			fit.table = a_table;
			measured = Measured(a_meter, fit, a_lines);
			if (measured && fit.wide > a_room && cuts) {
				measured = CutNames(a_meter, fit, a_room, a_lines);
			}
		}
		return measured ? std::optional{ fit } : std::nullopt;
	}
}
