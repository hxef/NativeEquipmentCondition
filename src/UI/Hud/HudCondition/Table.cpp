#include "UI/Hud/HudCondition/Table.h"

#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Roles/Hud.h"

#include <algorithm>
#include <optional>
#include <string>

namespace HudCondition
{
	namespace
	{
		using Scaleform::GFx::Value;
		namespace Readout = HudParts::Readout;

		// The word's size and width, as Spot.cpp writes it.
		constexpr double WORD_SIZE = 20.0;
		constexpr double WORD_WIDE = 1.0;

		// How high the HUD's bold font draws its capitals and digits, as a
		// share of its line's ascent, the rest being room for accents. A line
		// box runs from the ascent's top to the baseline, so a line's digits
		// start this far up from its baseline. Roboto Condensed's, the font
		// vanilla gives the HUD.
		constexpr double DIGITS_OF_ASCENT = 0.77;

		// Where the digits of a line start, its box running from the top of
		// its ascent to its baseline, as Roles::Hud::Numbers measures.
		[[nodiscard]] double DigitsTop(const Flash::Box& a_line)
		{
			return a_line.bottom - (DIGITS_OF_ASCENT * (a_line.bottom - a_line.top));
		}

		// a_place with its field's top left corner at 0,0.
		[[nodiscard]] Readout::LabelPlace FromCorner(const Readout::LabelPlace& a_place)
		{
			auto moved = a_place;
			moved.field = a_place.field.Moved(-a_place.field.left, -a_place.field.top);
			moved.letters = a_place.letters.Moved(-a_place.field.left, -a_place.field.top);
			return moved;
		}

		// The widest numbers CND decimals writes now, joined, which a table
		// is laid out for.
		[[nodiscard]] std::string WidestNow()
		{
			std::string widest;
			for (const auto tenths : Readout::WIDEST) {
				widest += Readout::PercentText(tenths) + " ";
			}
			return widest;
		}
	}

	// -------------------------------------------------------------------
	// Reading the rows
	// -------------------------------------------------------------------

	Rows RowsNow(Scaleform::GFx::Movie& a_movie, Value& a_counter, std::optional<bool> a_ownVisible, const Rows& a_last)
	{
		const auto divider = Roles::Hud::Divider(a_movie, a_counter, a_ownVisible);
		if (!divider || divider->turn != 0.0) {
			return a_last;
		}
		const auto& place = divider->place;
		const auto& line = divider->box;
		Rows        rows{ line.top - place.y, line.bottom - place.y, line.right - place.x, a_last.foot, a_last.head };

		// The nearest number over the line's middle and the nearest under
		// it, shown or not, so a number that drops out for a moment moves
		// nothing. A number the movie does not draw on either side keeps
		// its last gap from the line.
		const auto              middle = (line.top + line.bottom) / 2.0;
		std::optional<Flash::Box> over;
		std::optional<Flash::Box> under;
		for (const auto& number : Roles::Hud::Numbers(a_counter)) {
			const auto& box = number.box;
			const auto  own = (box.top + box.bottom) / 2.0;
			if (own < middle && (!over || box.bottom > over->bottom)) {
				over = box;
			}
			if (own > middle && (!under || box.top < under->top)) {
				under = box;
			}
		}
		rows.foot = over ? over->bottom - place.y : rows.top - (a_last.top - a_last.foot);
		rows.head = under ? DigitsTop(*under) - place.y : rows.bottom + (a_last.head - a_last.bottom);
		return rows;
	}

	// -------------------------------------------------------------------
	// Laying the 2 lines out
	// -------------------------------------------------------------------

	std::optional<Table> LayTable(Scaleform::GFx::Movie& a_movie, Value& a_readout, const Readout::LabelPlace& a_word, const Rows& a_rows, const std::optional<Table>& a_last)
	{
		if (!a_word.measured) {
			return std::nullopt;
		}
		const auto widest = WidestNow();
		if (a_last && a_last->rows == a_rows && a_last->word == a_word.letters && a_last->widest == widest) {
			return a_last;
		}
		auto field = Readout::ExtraOf(a_movie, a_readout, Readout::Extra::kNumber);
		if (!field.IsDisplayObject()) {
			return std::nullopt;
		}

		// The number is laid out at its widest, so nothing moves as its
		// digits change.
		std::optional<Readout::LabelPlace> wide;
		for (const auto tenths : Readout::WIDEST) {
			const auto at = Readout::WriteText(field, Readout::PercentText(tenths), WORD_SIZE, WORD_WIDE);
			if (!at.measured) {
				return std::nullopt;
			}
			if (!wide || at.letters.right - at.letters.left > wide->letters.right - wide->letters.left) {
				wide = FromCorner(at);
			}
		}

		Table table{ {}, a_rows.foot - a_word.letters.bottom, {}, a_rows, a_word.letters, widest };
		table.number = Flash::Point{ a_word.letters.left - wide->letters.left, a_rows.head - DigitsTop(wide->letters) };
		const auto word = a_word.letters.Moved(0.0, table.rise);
		const auto number = wide->letters.Moved(table.number.x, table.number.y);
		const auto fields = a_word.field.Moved(0.0, table.rise).With(wide->field.Moved(table.number.x, table.number.y));
		table.place = Readout::LabelPlace{ fields, word.With(number), true, wide->descent };
		return table;
	}

	void PutTable(Value& a_readout, const Readout::LabelPlace& a_word, const Table& a_table, double a_x, double a_y)
	{
		auto label = Readout::LabelOf(a_readout);
		auto number = Readout::ExtraIn(a_readout, Readout::Extra::kNumber);
		auto line = Readout::ExtraIn(a_readout, Readout::Extra::kLine);
		Readout::MoveField(label, a_word.field.left + a_x, a_word.field.top + a_table.rise + a_y);
		Readout::MoveField(number, a_table.number.x + a_x, a_table.number.y + a_y);
		Flash::Set(number, "visible"sv, Value(true));
		Flash::Set(line, "visible"sv, Value(false));
	}

	void WriteNumber(Value& a_readout, const std::string& a_number)
	{
		auto number = Readout::ExtraIn(a_readout, Readout::Extra::kNumber);
		static_cast<void>(Readout::WriteText(number, a_number, WORD_SIZE, WORD_WIDE));
	}

	void DrawLine(Scaleform::GFx::Movie& a_movie, Value& a_readout, const Table& a_table, bool a_lined)
	{
		auto line = a_lined ? Readout::ExtraOf(a_movie, a_readout, Readout::Extra::kLine) : Readout::ExtraIn(a_readout, Readout::Extra::kLine);
		if (!line.IsDisplayObject()) {
			return;
		}
		auto       label = Readout::LabelOf(a_readout);
		auto       number = Readout::ExtraIn(a_readout, Readout::Extra::kNumber);
		const auto right = std::max(Readout::ExtentOf(label).letters.right, Readout::ExtentOf(number).letters.right);
		const auto& rows = a_table.rows;
		const auto  shown = a_lined && right > rows.end;
		if (shown) {
			HudParts::PutBox(line, rows.end, rows.top, right - rows.end, rows.bottom - rows.top);
		}
		Flash::Set(line, "visible"sv, Value(shown));
	}

	void FadeLine(Value& a_readout, double a_alpha)
	{
		auto line = Readout::ExtraIn(a_readout, Readout::Extra::kLine);
		Flash::Set(line, "alpha"sv, Value(a_alpha));
	}

	void HideTable(Value& a_readout)
	{
		auto number = Readout::ExtraIn(a_readout, Readout::Extra::kNumber);
		auto line = Readout::ExtraIn(a_readout, Readout::Extra::kLine);
		Flash::Set(number, "visible"sv, Value(false));
		Flash::Set(line, "visible"sv, Value(false));
	}
}
