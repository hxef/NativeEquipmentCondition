#include "UI/Repair/Columns/Columns.h"

#include "Core/TraceLog.h"
#include "UI/MessageBox.h"
#include "UI/Repair/Columns/Cells.h"
#include "UI/Repair/Columns/Fit.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <format>

namespace Columns
{
	namespace
	{
		// How many boxes left out their gains since the game started, and the
		// count NEC.log last gave.
		std::atomic<std::uint32_t> g_leftOut{ 0 };
		std::atomic<std::uint32_t> g_leftOutSaid{ 0 };

		// The end of every row in columns: a last tab to the block's end, then
		// a no-break space at size 1. A centred line is centred by its width
		// without the tabs and spaces it ends in, and a no-break space counts,
		// so every row is as wide as the block and all start at 1 left edge.
		constexpr std::string_view ROW_END{ "\t<font size=\"1\">&nbsp;</font>" };

		// The room that space takes, less than 1 px, kept free so a row as
		// wide as the room does not wrap it onto a line of its own.
		constexpr double ROW_END_ROOM = 1.0;

		// -------------------------------------------------------------------
		// The trace
		// -------------------------------------------------------------------

		// Every row joined for the trace, a tab spelled \t.
		[[nodiscard]] std::string Traced(const std::vector<std::string>& a_rows)
		{
			std::string out;
			for (const auto& row : a_rows) {
				out += out.empty() ? "" : " | ";
				for (const auto ch : row) {
					out += ch == '\t' ? std::string_view{ "\\t" } : std::string_view{ &ch, 1 };
				}
			}
			return out;
		}

		// The steps a_fit took, for the trace.
		[[nodiscard]] std::string Steps(const Fit& a_fit, bool a_gains)
		{
			std::string out{ "with no gains to show" };
			if (a_fit.leftOut) {
				out = std::format("the gains left out at {:.0f} px", *a_fit.leftOut);
			} else if (a_gains) {
				out = a_fit.shortWords ? "with the gains in short words" : "with the gains";
			}
			if (a_fit.size < ROW_SIZE) {
				out += std::format(", its cells at size {:d}", a_fit.size);
			}
			if (a_fit.cut > 0) {
				out += std::format(" with {:d} {:s} cut", a_fit.cut, a_fit.cut == 1 ? "name" : "names");
			}
			return out;
		}

		// -------------------------------------------------------------------
		// The rows as the box takes them
		// -------------------------------------------------------------------

		// a_line as a centred paragraph, as the rows in columns are one, so a
		// field counts it as 2 lines while it draws 1 and only a wrap counts more.
		[[nodiscard]] std::string Centred(const std::string& a_line)
		{
			return std::format(R"(<p align="center">{:s}</p>)", a_line);
		}

		// The rows as 1 line each at the row's own size, every cell with
		// letters in it, for a box nothing could be measured for.
		[[nodiscard]] std::vector<std::string> Plain(const Table& a_table, std::string_view a_box)
		{
			std::vector<std::string> out;
			for (const auto& row : a_table.rows) {
				std::vector<std::size_t> every;
				for (std::size_t column = 0; column < row.size(); column++) {
					every.push_back(column);
				}
				out.push_back(Centred(Lined(a_table.kept, row, every, ROW_SIZE)));
			}
			TraceLog::Line("menu", "{:s} could not measure its cells, so its rows are plain and centred: {:s}", a_box, Traced(out));
			return out;
		}

		// The rows in columns, each a centred paragraph as wide as the block,
		// so they are centred as vanilla's rows are and keep 1 left edge, in
		// vanilla's field and in a message box mod's field sized to its rows
		// alike. A stop counts from where the row starts. Vanilla's box
		// centres the lit bar on a row's line, so the bar sits on the block.
		// The trace gives the room left on each side of the block in a field
		// the room wide, and each stop from that field's edge, that room first.
		[[nodiscard]] std::vector<std::string> InColumns(const Fit& a_fit, double a_room, std::string_view a_box, const std::string& a_steps)
		{
			const auto  laid = StopsOf(a_fit.columns, a_fit.shown);
			const auto  side = std::max(std::floor((a_room - laid.wide) / 2.0), 0.0);
			std::string stops;
			std::string fromEdge = side > 0.0 ? std::format("{:.0f}", side) : std::string{};
			for (const auto at : laid.at) {
				stops += std::format("{:.0f},", at);
				fromEdge += std::format("{:s}{:.0f}", fromEdge.empty() ? "" : ",", side + at);
			}
			stops += std::format("{:.0f}", std::ceil(laid.wide));

			std::vector<std::string> out;
			for (const auto& row : a_fit.table.rows) {
				const auto cells = Tabbed(a_fit.table.kept, row, a_fit.shown, a_fit.size);
				out.push_back(std::format(R"(<p align="center"><textformat tabstops="[{:s}]">{:s}{:s}</textformat></p>)", stops, cells, ROW_END));
			}
			TraceLog::Line("menu", "{:s} lays its rows in {:d} columns, centred with {:.0f} px left on each side in a field the room wide, at tab stops [{:s}], {:.0f} px apart, the gains at size {:d}, {:.0f} px wide for a room of {:.0f} px, {:s}: {:s}",
				a_box, a_fit.shown.size(), side, fromEdge, GAP, GAIN_SIZE, laid.wide, a_room, a_steps, Traced(out));
			return out;
		}
	}

	std::vector<std::string> Lay(Scaleform::GFx::Movie* a_movie, const Table& a_table, std::string_view a_box)
	{
		std::optional<Meter> meter;
		if (a_movie) {
			meter.emplace(*a_movie);
		}
		const auto widths = meter ? Measure(*meter, a_table, ROW_SIZE) : std::nullopt;
		if (!widths) {
			return Plain(a_table, a_box);
		}

		// A column empty on every row, such as the mark when no spare has
		// one, takes no room and no tab.
		const auto               wide = Widest(*widths);
		std::vector<std::size_t> all;
		std::vector<std::size_t> kept;
		for (std::size_t column = 0; column < wide.size(); column++) {
			if (wide[column] > 0.0) {
				all.push_back(column);
				if (column < a_table.kept) {
					kept.push_back(column);
				}
			}
		}
		// Whole pixels, so a closing stop rounded up leaves ROW_END_ROOM free.
		const auto room = std::floor(MessageBox::RowRoom());
		const auto fit = Fitted(*meter, a_table, all, kept, room - ROW_END_ROOM);
		if (!fit) {
			return Plain(a_table, a_box);
		}
		if (fit->leftOut && g_leftOut.fetch_add(1) == 0) {
			REX::INFO("{:s} left out its gains, as its widest row passes the room of {:.0f} px even at size {:d}. The boxes after it are counted when the main menu opens, a save loads or a new game begins.",
				a_box, room, SMALLEST_SIZE);
		}
		const auto steps = Steps(*fit, all.size() > kept.size());
		return InColumns(*fit, room, a_box, steps);
	}

	void SayLeftOut()
	{
		const auto count = g_leftOut.load();
		if (count > 1 && g_leftOutSaid.exchange(count) != count) {
			REX::INFO("{:d} repair boxes have left out their gains since the game started.", count);
		}
	}
}
