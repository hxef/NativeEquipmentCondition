#include "UI/Repair/Columns/Cells.h"

#include "UI/Flash.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace Columns
{
	namespace
	{
		// a_cell escaped for html, in a font tag of its own when it is not at
		// the row's own size.
		[[nodiscard]] std::string Sized(const std::string& a_cell, int a_size)
		{
			const auto cell = Flash::HtmlEscaped(a_cell);
			return a_size == ROW_SIZE || cell.empty() ? cell : std::format(R"(<font size="{:d}">{:s}</font>)", a_size, cell);
		}

		[[nodiscard]] int SizeOf(std::size_t a_kept, std::size_t a_column, int a_size)
		{
			return a_column < a_kept ? a_size : GAIN_SIZE;
		}
	}

	// -----------------------------------------------------------------------
	// Measuring the cells
	// -----------------------------------------------------------------------

	Meter::Meter(Scaleform::GFx::Movie& a_movie)
	{
		using Scaleform::GFx::Value;
		a_movie.CreateObject(&field, "flash.text.TextField");
		ready = field.IsDisplayObject() && Flash::Set(field, "embedFonts"sv, Value(true)) && Flash::Set(field, "autoSize"sv, Value("left"));
	}

	std::optional<double> Meter::Width(const std::string& a_html)
	{
		if (!ready) {
			return std::nullopt;
		}
		const auto [at, fresh] = known.try_emplace(a_html, 0.0);
		if (fresh && !a_html.empty()) {
			const auto html = std::format(R"(<font face="{:s}" size="{:d}" letterSpacing="0" kerning="0">{:s}</font>)", ROW_FONT, ROW_SIZE, a_html);
			at->second = Flash::Set(field, "htmlText"sv, Scaleform::GFx::Value(html.c_str())) ? Flash::Number(field, "textWidth"sv) : 0.0;
		}
		if (!a_html.empty() && !(at->second > 0.0)) {
			return std::nullopt;
		}
		return at->second;
	}

	std::optional<double> Meter::Cell(const std::string& a_cell, int a_size)
	{
		return Width(Sized(a_cell, a_size));
	}

	std::optional<std::vector<std::vector<double>>> Measure(Meter& a_meter, const Table& a_table, int a_size)
	{
		std::vector<std::vector<double>> out;
		for (const auto& row : a_table.rows) {
			auto& widths = out.emplace_back();
			for (std::size_t column = 0; column < row.size(); column++) {
				const auto width = a_meter.Cell(row[column], SizeOf(a_table.kept, column, a_size));
				if (!width) {
					return std::nullopt;
				}
				widths.push_back(*width);
			}
		}
		return out;
	}

	std::vector<double> Widest(const std::vector<std::vector<double>>& a_widths)
	{
		std::vector<double> out;
		for (const auto& row : a_widths) {
			out.resize(std::max(out.size(), row.size()), 0.0);
			for (std::size_t column = 0; column < row.size(); column++) {
				out[column] = std::max(out[column], row[column]);
			}
		}
		return out;
	}

	// -----------------------------------------------------------------------
	// The rows
	// -----------------------------------------------------------------------

	std::string Tabbed(std::size_t a_kept, const std::vector<std::string>& a_cells, const std::vector<std::size_t>& a_shown, int a_size)
	{
		std::string out;
		for (const auto column : a_shown) {
			out += column == a_shown.front() ? "" : "\t";
			out += column < a_cells.size() ? Sized(a_cells[column], SizeOf(a_kept, column, a_size)) : std::string{};
		}
		return out;
	}

	std::string Lined(std::size_t a_kept, const std::vector<std::string>& a_cells, const std::vector<std::size_t>& a_shown, int a_size)
	{
		std::string out;
		for (const auto column : a_shown) {
			if (column < a_cells.size() && !a_cells[column].empty()) {
				out += out.empty() ? "" : LINE_GAP;
				out += Sized(a_cells[column], SizeOf(a_kept, column, a_size));
			}
		}
		return out;
	}

	Stops StopsOf(const std::vector<double>& a_wide, const std::vector<std::size_t>& a_shown)
	{
		Stops  out;
		double at = 0.0;
		for (std::size_t i = 1; i < a_shown.size(); i++) {
			at += a_wide[a_shown[i - 1]] + GAP;
			out.at.push_back(std::ceil(at));
		}
		if (!a_shown.empty()) {
			out.wide = (out.at.empty() ? 0.0 : out.at.back()) + a_wide[a_shown.back()];
		}
		return out;
	}
}
