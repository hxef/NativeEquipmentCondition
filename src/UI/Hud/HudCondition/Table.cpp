#include "UI/Hud/HudCondition/Table.h"

#include "Core/Settings/Settings.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Roles/Hud.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

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

		// Where the game draws a_number's baseline, in the counter's units:
		// the top of its line as Numbers measures it, moved down by the
		// line's ascent and by how far the game draws the line lower. The
		// box Numbers gives runs on to the lowest the baseline could sit,
		// which in a field taller than its text, as vanilla's are, is under
		// the digits. Never lower than that bottom, which a turned or
		// mirrored field or a line that cannot be measured keeps. Nothing
		// for a field with no text, since no probe finds where an empty
		// line is drawn, so the word keeps its place.
		[[nodiscard]] std::optional<double> Baseline(Value& a_counter, const Roles::Hud::Part& a_number)
		{
			auto field = Flash::Child(a_counter, a_number.name.c_str());
			if (Flash::String(field, "text"sv).empty()) {
				return std::nullopt;
			}
			const auto line = Flash::Number(field, "rotation"sv) == 0.0 ? Flash::DrawnLine(field) : std::nullopt;
			const auto down = Flash::Number(field, "scaleY"sv, 1.0);
			const auto baseline = line ? a_number.box.top + ((line->drop + line->baseline - line->top) * down) : a_number.box.bottom;
			return down > 0.0 && baseline < a_number.box.bottom ? baseline : a_number.box.bottom;
		}

		// a_place with its field's top left corner at 0,0.
		[[nodiscard]] Readout::LabelPlace FromCorner(const Readout::LabelPlace& a_place)
		{
			auto moved = a_place;
			moved.field = a_place.field.Moved(-a_place.field.left, -a_place.field.top);
			moved.letters = a_place.letters.Moved(-a_place.field.left, -a_place.field.top);
			return moved;
		}

		// The 1 line for a_tenths, CND 87.4%, or the number alone while
		// bHudBarShowsWord is off.
		[[nodiscard]] std::string LineText(Value& a_field, std::uint32_t a_tenths, bool a_worded)
		{
			return a_worded ? Readout::WordAndPercent(a_field, a_tenths) : Readout::PercentText(a_tenths);
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

		// Where a_field's letters sit, from its top left corner, with the
		// widest text a_text gives for the numbers of WIDEST written in.
		// Nothing while a line cannot be measured. With a_whole its letters
		// reach as far as the wider of the 2 widths the movie gives the text,
		// its line's and textWidth, which differ by about half a unit, so a
		// line held by the screen's edge stays whole on screen by either.
		template <class Text>
		[[nodiscard]] std::optional<Readout::LabelPlace> Widest(Value& a_field, Text a_text, bool a_whole = false)
		{
			std::optional<Readout::LabelPlace> wide;
			for (const auto tenths : Readout::WIDEST) {
				auto at = Readout::WriteText(a_field, a_text(tenths), WORD_SIZE, WORD_WIDE);
				if (!at.measured) {
					return std::nullopt;
				}
				if (a_whole) {
					const auto text = Flash::Number(a_field, "textWidth"sv) * Flash::Number(a_field, "scaleX"sv, 1.0);
					at.letters.right = std::max(at.letters.right, at.letters.left + text);
				}
				if (!wide || at.letters.right - at.letters.left > wide->letters.right - wide->letters.left) {
					wide = FromCorner(at);
				}
			}
			return wide;
		}

		// The 1 line at the ammo line is never drawn under this share of the
		// word's size. Where even that runs off screen, Spot.h finds it a
		// free spot as it does for the word.
		constexpr double MIN_LINE_SCALE = 0.6;

		// How far the 1 line at the ammo line ends before the screen's edge.
		constexpr double LINE_ROOM = 0.5;

		// The share of the word's size the 1 line with a_letters, at its
		// widest in the word's size, takes to end by a_right. 1 with no edge
		// known. In steps of 0.01, so an edge that moves by a hair lays
		// nothing out again.
		[[nodiscard]] double LineScale(const Flash::Box& a_letters, std::optional<double> a_right)
		{
			const auto wide = a_letters.right - a_letters.left;
			if (!a_right || wide <= 0.0) {
				return 1.0;
			}
			const auto scale = std::floor(((*a_right - LINE_ROOM - a_letters.left) / wide) * 100.0) / 100.0;
			return std::clamp(scale, MIN_LINE_SCALE, 1.0);
		}

		// a_line, measured with its field's corner at 0,0, drawn a_scale
		// times its size with its field's corner moved from a_at so its
		// letters keep the left edge and the baseline they have at a_at. A
		// field scales about that corner.
		[[nodiscard]] OneLine Shrunk(const Readout::LabelPlace& a_line, const Flash::Point& a_at, double a_scale)
		{
			const auto         by = [&](const Flash::Box& a_box) { return Flash::Box{ a_box.left * a_scale, a_box.top * a_scale, a_box.right * a_scale, a_box.bottom * a_scale }; };
			const Flash::Point corner{ a_at.x + ((1.0 - a_scale) * a_line.letters.left), a_at.y + ((1.0 - a_scale) * a_line.letters.bottom) };
			const Readout::LabelPlace place{ by(a_line.field).Moved(corner.x, corner.y), by(a_line.letters).Moved(corner.x, corner.y), true, a_line.descent * a_scale };
			return OneLine{ place, corner, a_scale };
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
		// its last gap from the line, as an empty one over it does. The word
		// stands on the baseline the number over the line is drawn on, see
		// Baseline.
		const auto                      middle = (line.top + line.bottom) / 2.0;
		std::optional<Roles::Hud::Part> over;
		std::optional<Flash::Box>       under;
		for (const auto& number : Roles::Hud::Numbers(a_counter)) {
			const auto& box = number.box;
			const auto  own = (box.top + box.bottom) / 2.0;
			if (own < middle && (!over || box.bottom > over->box.bottom)) {
				over = number;
			}
			if (own > middle && (!under || box.top < under->top)) {
				under = box;
			}
		}
		const auto foot = over ? Baseline(a_counter, *over) : std::nullopt;
		rows.foot = foot ? *foot - place.y : rows.top - (a_last.top - a_last.foot);
		rows.head = under ? DigitsTop(*under) - place.y : rows.bottom + (a_last.head - a_last.bottom);
		return rows;
	}

	// -------------------------------------------------------------------
	// Laying the lines out
	// -------------------------------------------------------------------

	std::optional<Table> LayTable(Scaleform::GFx::Movie& a_movie, Value& a_readout, const Readout::LabelPlace& a_word, const Rows& a_rows, const std::optional<Table>& a_last,
		std::optional<double> a_lineRight)
	{
		if (!a_word.measured) {
			return std::nullopt;
		}
		const auto widest = WidestNow();
		const auto worded = Settings::bHudBarShowsWord.GetValue();
		if (a_last && a_last->rows == a_rows && a_last->word == a_word.letters && a_last->widest == widest && a_last->worded == worded &&
			LineScale(a_last->away.place.letters, a_lineRight) == a_last->atLine.scale) {
			return a_last;
		}
		auto field = Readout::ExtraOf(a_movie, a_readout, Readout::Extra::kNumber);
		if (!field.IsDisplayObject()) {
			return std::nullopt;
		}

		// The 1 line and the number are laid out at their widest, so nothing
		// moves as the digits change. The number goes last, so the field is
		// left holding a number.
		const auto line = Widest(field, [&](std::uint32_t a_tenths) { return LineText(field, a_tenths, worded); }, true);
		const auto wide = Widest(field, [](std::uint32_t a_tenths) { return Readout::PercentText(a_tenths); });
		if (!line || !wide) {
			return std::nullopt;
		}

		Table table{ {}, {}, {}, a_rows.foot - a_word.letters.bottom, {}, a_rows, a_word.letters, widest, worded };
		table.number = Flash::Point{ a_word.letters.left - wide->letters.left, a_rows.head - DigitsTop(wide->letters) };
		const auto word = a_word.letters.Moved(0.0, table.rise);
		const auto number = wide->letters.Moved(table.number.x, table.number.y);
		const auto fields = a_word.field.Moved(0.0, table.rise).With(wide->field.Moved(table.number.x, table.number.y));
		table.place = Readout::LabelPlace{ fields, word.With(number), true, wide->descent };

		// The 1 line is written in the word's own field, where the word's
		// field stands.
		const Flash::Point at{ a_word.field.left, a_word.field.top + table.rise };
		table.away = Shrunk(*line, at, 1.0);
		table.atLine = Shrunk(*line, at, LineScale(table.away.place.letters, a_lineRight));
		return table;
	}

	// -------------------------------------------------------------------
	// 1 line or 2
	// -------------------------------------------------------------------

	Lines LinesFor(const LinesFacts& a_facts)
	{
		if (a_facts.moved) {
			return Lines{ .one = true, .why = "since fHudBarX or fHudBarY moved it"sv };
		}
		if (a_facts.alone) {
			return Lines{ .one = true, .why = "since it stands alone"sv };
		}
		if (a_facts.noCounter) {
			return Lines{ .one = true, .why = "since the game shows no ammo counter for this weapon"sv };
		}
		if (!a_facts.between) {
			return Lines{ .one = true, .why = "since no ammo divider runs between the 2 ammo numbers"sv };
		}
		if (a_facts.wordless) {
			return Lines{ .atLine = true, .one = true, .why = "since bHudBarShowsWord is off"sv };
		}
		if (a_facts.asked) {
			return Lines{ .atLine = true, .one = true, .why = "since bHudBarPercentOneLine is on"sv };
		}
		if (!a_facts.fits) {
			return Lines{ .atLine = true, .one = true, .why = "since the 2 lines have no room by the ammo divider"sv };
		}
		return Lines{ .atLine = true, .why = "over and under the ammo divider"sv };
	}

	// -------------------------------------------------------------------
	// Drawing the lines
	// -------------------------------------------------------------------

	void PutTable(Value& a_readout, const Readout::LabelPlace& a_word, const Table& a_table, double a_x, double a_y, const OneLine* a_one)
	{
		auto       label = Readout::LabelOf(a_readout);
		auto       number = Readout::ExtraIn(a_readout, Readout::Extra::kNumber);
		auto       line = Readout::ExtraIn(a_readout, Readout::Extra::kLine);
		const auto corner = a_one ? a_one->corner : Flash::Point{ a_word.field.left, a_word.field.top + a_table.rise };
		Readout::MoveField(label, corner.x + a_x, corner.y + a_y);
		Readout::MoveField(number, a_table.number.x + a_x, a_table.number.y + a_y);
		Flash::Set(number, "visible"sv, Value(!a_one));
		Flash::Set(line, "visible"sv, Value(false));
	}

	void WriteNumber(Value& a_readout, const std::string& a_number)
	{
		auto number = Readout::ExtraIn(a_readout, Readout::Extra::kNumber);
		static_cast<void>(Readout::WriteText(number, a_number, WORD_SIZE, WORD_WIDE));
	}

	void WriteOneLine(Value& a_readout, std::uint32_t a_tenths, double a_scale)
	{
		auto label = Readout::LabelOf(a_readout);
		static_cast<void>(Readout::WriteText(label, LineText(label, a_tenths, Settings::bHudBarShowsWord.GetValue()), WORD_SIZE * a_scale, WORD_WIDE));
	}

	void WriteWord(Value& a_readout)
	{
		auto label = Readout::LabelOf(a_readout);
		static_cast<void>(Readout::WriteText(label, HudParts::Translated(label, HudParts::CND_TEXT), WORD_SIZE, WORD_WIDE));
	}

	void DrawLine(Scaleform::GFx::Movie& a_movie, Value& a_readout, const Table& a_table, bool a_lined, bool a_oneLine)
	{
		auto line = a_lined ? Readout::ExtraOf(a_movie, a_readout, Readout::Extra::kLine) : Readout::ExtraIn(a_readout, Readout::Extra::kLine);
		if (!line.IsDisplayObject()) {
			return;
		}
		auto       label = Readout::LabelOf(a_readout);
		auto       number = Readout::ExtraIn(a_readout, Readout::Extra::kNumber);
		const auto ends = Readout::ExtentOf(label).letters.right;
		const auto right = a_oneLine ? ends : std::max(ends, Readout::ExtentOf(number).letters.right);
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
