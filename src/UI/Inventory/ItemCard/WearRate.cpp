#include "UI/Inventory/ItemCard/Cards.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/WeaponWear/WeaponWear.h"
#include "Core/IniText.h"
#include "Core/TraceLog.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "UI/Flash.h"
#include "UI/Roles/Card.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ItemCard
{
	namespace
	{
		using Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// The text
		// -------------------------------------------------------------------

		// What NEC writes after the percent in the number's own field.
		constexpr std::string_view WEAR_RATE_START = " (x";

		// The rate as the row prints it, (x1.3): 1 decimal, 2 below 0.05 and a
		// whole number from 100 up, x0 for an item that never wears and x0.01
		// at the least for one that does. The point is "." in every language,
		// as the card prints its other numbers, and nothing is translated.
		std::string WearRateText(float a_rate)
		{
			if (!(a_rate > 0.0F)) {
				return "(x0)";
			}
			if (a_rate < 0.005F) {
				return "(x0.01)";
			}
			if (a_rate < 0.05F) {
				return std::format("(x{:.2f})", a_rate);
			}
			if (a_rate < 99.95F) {
				return std::format("(x{:.1f})", a_rate);
			}
			return std::format("(x{:.0f})", a_rate);
		}

		// The rate in the cell beside the number, which is narrower: a whole
		// number from 10 up, (x18) for 17.6, (x.01) for 0.01, else as
		// WearRateText writes it.
		std::string CellWearRateText(float a_rate)
		{
			// Below 0.05 the 0 before the point goes, since the narrowest cell,
			// 46 wide at Roboto Condensed 18 in one replacer, holds (x.01) at 39
			// and not (x0.01) at 47.5.
			if (a_rate > 0.0F && a_rate < 0.05F) {
				return "(x" + WearRateText(a_rate).substr(3);  // (x0.01) to (x.01)
			}
			return a_rate < 9.95F ? WearRateText(a_rate) : std::format("(x{:.0f})", a_rate);
		}

		// The sign after a rate printed as a_text: + when the item would wear
		// faster than the item compared, printed as a_comparedText, - when
		// slower, and nothing when both print the same.
		std::string_view Sign(const std::string& a_text, const std::string& a_comparedText, float a_rate, float a_compared)
		{
			if (a_text == a_comparedText) {
				return {};
			}
			return a_rate > a_compared ? "+"sv : "-"sv;
		}

		// The texts the number's field may show after the percent for a_rate,
		// best first: with the sign, then without, so a field too narrow for
		// the sign still shows the rate.
		std::vector<std::string> FieldTries(float a_rate, std::optional<float> a_compared)
		{
			auto       text = WearRateText(a_rate);
			const auto sign = a_compared ? Sign(text, WearRateText(*a_compared), a_rate, *a_compared) : ""sv;
			if (sign.empty()) {
				return { text };
			}
			return { std::format("{:s} {:s}", text, sign), text };
		}

		// The same for the cell beside the number, which also tries the sign
		// without the brackets, x1.3 +, since a narrow cell, 46 wide in one
		// replacer, cannot hold (x1.3) +. The brackets stay while no sign shows.
		std::vector<std::string> CellTries(float a_rate, std::optional<float> a_compared)
		{
			auto       text = CellWearRateText(a_rate);
			const auto sign = a_compared ? Sign(text, CellWearRateText(*a_compared), a_rate, *a_compared) : ""sv;
			if (sign.empty()) {
				return { text };
			}
			const auto bare = std::string_view{ text }.substr(1, text.size() - 2);
			return { std::format("{:s} {:s}", text, sign), std::format("{:s} {:s}", bare, sign), text };
		}

		// Whether a cell's a_text is a rate NEC wrote, (x1.3), x1.3 + or
		// x.01 +, and not a difference the movie prints itself, such as +8.5.
		bool IsWearRateText(std::string_view a_text)
		{
			if (a_text.starts_with("(x"sv)) {
				return true;
			}
			return a_text.size() > 1 && a_text[0] == 'x' && (a_text[1] == '.' || std::isdigit(static_cast<unsigned char>(a_text[1])));
		}

		// The movie's name for the trace, PipboyMenu for PipboyMenu.swf.
		std::string_view MovieName(std::string_view a_file)
		{
			constexpr auto SWF = ".swf"sv;
			return a_file.ends_with(SWF) ? a_file.substr(0, a_file.size() - SWF.size()) : a_file;
		}

		// -------------------------------------------------------------------
		// Measuring
		// -------------------------------------------------------------------

		// At least this much stays clear between the label's last letter and
		// the number's field, in the row's units, about a space and a half on
		// the vanilla card.
		constexpr double LABEL_GAP = 8.0;

		// Room left over in a field that holds the text at its natural size,
		// so rounding never tips it into shrinking.
		constexpr double SLACK = 1.0;

		// How far 2 measures may differ and still be the same size.
		constexpr double SAME = 0.5;

		struct Size
		{
			double width;
			double height;
		};

		// The size a_text takes at its natural size in a_format, from a field
		// of NEC's own that is never shown, since a card's own fields shrink
		// their font to fit. Nothing when the movie refuses it.
		std::optional<Size> NaturalSize(Scaleform::GFx::Movie& a_movie, const Value& a_format, const std::string& a_text)
		{
			Value probe;
			a_movie.CreateObject(&probe, "flash.text.TextField");
			// The game's fonts live in its font library, which is what
			// embedFonts asks for.
			const auto made = probe.IsDisplayObject() && Flash::Set(probe, "embedFonts"sv, Value(true)) &&
			                  Flash::Set(probe, "wordWrap"sv, Value(false)) && Flash::Set(probe, "autoSize"sv, Value("left")) &&
			                  Flash::Set(probe, "defaultTextFormat"sv, a_format) && Flash::Set(probe, "text"sv, Value(a_text.c_str()));
			if (!made) {
				return std::nullopt;
			}
			const auto nan = std::numeric_limits<double>::quiet_NaN();
			const Size size{ Flash::Number(probe, "textWidth"sv, nan), Flash::Number(probe, "textHeight"sv, nan) };
			return std::isfinite(size.width) && std::isfinite(size.height) && size.width > 0.0 ? std::optional{ size } : std::nullopt;
		}

		// A field's format as its text holds it, or nothing.
		std::optional<Value> FormatOf(Value& a_field)
		{
			Value format;
			if (!a_field.IsDisplayObject() || !a_field.Invoke("getTextFormat", &format) || !format.IsObject()) {
				return std::nullopt;
			}
			return format;
		}

		// Writes a_text into a_field in a_format, as the movie wrote the text
		// before it. True when it shows at a_height and fits a_width, so the
		// field did not shrink it.
		bool WriteShown(Value& a_field, const Value& a_format, const std::string& a_text, double a_height, double a_width)
		{
			if (!Flash::Set(a_field, "text"sv, Value(a_text.c_str()))) {
				return false;
			}
			Flash::Call(a_field, "setTextFormat", std::span{ &a_format, 1 });
			const auto nan = std::numeric_limits<double>::quiet_NaN();
			const auto height = Flash::Number(a_field, "textHeight"sv, nan);
			const auto width = (Flash::Number(a_field, "textWidth"sv, nan) + 2.0 * Flash::FIELD_MARGIN) * Flash::Number(a_field, "scaleX"sv, 1.0);
			return std::abs(height - a_height) <= SAME && width <= a_width + SAME;
		}

		// -------------------------------------------------------------------
		// The 3 forms
		// -------------------------------------------------------------------

		// Form A: a_text, the percent and the rate, in the number's own field
		// at its natural size. The field grows left with its right edge kept,
		// only while the row shows nothing but its background in that stretch,
		// and its text starts no further left than the label's last letter and
		// LABEL_GAP, wherever the field's box starts. The text is taken as
		// right aligned, as vanilla's number is, which puts it furthest left.
		// How much wider it grew, or nothing when it would not fit or would
		// shrink, with the field as it was.
		std::optional<double> WriteInField(Scaleform::GFx::Movie& a_movie, Value& a_row, Value& a_field, const std::string& a_printed,
			const std::string& a_text)
		{
			auto       label = Roles::Card::LabelField(a_row);
			const auto line = label.IsDisplayObject() ? Flash::FirstLine(label) : std::nullopt;
			const auto drawn = label.IsDisplayObject() ? Roles::BoundsIn(label, a_row) : std::nullopt;
			const auto own = label.IsDisplayObject() ? Roles::BoundsIn(label, label) : std::nullopt;
			const auto format = FormatOf(a_field);
			const auto natural = format ? NaturalSize(a_movie, *format, a_text) : std::nullopt;
			if (!line || !drawn || !own || !natural) {
				return std::nullopt;
			}

			const auto nan = std::numeric_limits<double>::quiet_NaN();
			const auto x = Flash::Number(a_field, "x"sv, nan);
			const auto width = Flash::Number(a_field, "width"sv, nan);
			const auto height = Flash::Number(a_field, "textHeight"sv, nan);
			const auto scale = Flash::Number(a_field, "scaleX"sv, 1.0);
			const auto labelEnd = drawn->left + (line->right - own->left) * (drawn->right - drawn->left) / (own->right - own->left);
			if (!std::isfinite(x) || !std::isfinite(width) || !std::isfinite(height) || !std::isfinite(labelEnd)) {
				return std::nullopt;
			}

			const auto room = (natural->width + 2.0 * Flash::FIELD_MARGIN + SLACK) * scale;
			const auto need = std::max(width, room);
			const auto left = x + width - need;
			if (x + width - room < labelEnd + LABEL_GAP || (need > width && Roles::Card::DrawsAcross(a_row, left, x))) {
				return std::nullopt;
			}

			Flash::Set(a_field, "width"sv, Value(need));
			Flash::Set(a_field, "x"sv, Value(left));
			if (WriteShown(a_field, *format, a_text, height, need)) {
				return need - width;
			}
			WriteShown(a_field, *format, a_printed, height, width);
			Flash::Set(a_field, "width"sv, Value(width));
			Flash::Set(a_field, "x"sv, Value(x));
			return std::nullopt;
		}

		// Form B: a_rate in a_cell, the empty field right of the number, at
		// the number's size and in its colour, aligned as the cell is. An
		// empty field's getTextFormat holds no font and no size, so the
		// format is the number's. False when it would not fit or would
		// shrink, with the cell left empty and a_why saying so in words.
		bool WriteInCell(Scaleform::GFx::Movie& a_movie, Value& a_field, Value& a_cell, const std::string& a_rate, std::string& a_why)
		{
			auto format = FormatOf(a_field);
			if (!format) {
				a_why = "gets no text format, as the number has none";
				return false;
			}
			if (const auto align = Flash::Member(Flash::Member(a_cell, "defaultTextFormat"sv), "align"sv); align.IsString()) {
				Flash::Set(*format, "align"sv, align);
			}
			const auto natural = NaturalSize(a_movie, *format, a_rate);
			if (!natural) {
				a_why = std::format("gets no size for \"{:s}\" from the movie", a_rate);
				return false;
			}
			const auto width = Flash::Number(a_cell, "width"sv);
			const auto scale = Flash::Number(a_cell, "scaleX"sv, 1.0);
			const auto need = (natural->width + 2.0 * Flash::FIELD_MARGIN) * scale;
			if (need > width) {
				a_why = std::format("is {:.1f} wide, and \"{:s}\" needs {:.1f}", width, a_rate, need);
				return false;
			}
			if (!WriteShown(a_cell, *format, a_rate, natural->height, width)) {
				const auto nan = std::numeric_limits<double>::quiet_NaN();
				a_why = std::format("is {:.1f} wide, and \"{:s}\" needs {:.1f} at {:.1f} high, but it drew {:.1f} wide at {:.1f} high", width,
					a_rate, need, natural->height, (Flash::Number(a_cell, "textWidth"sv, nan) + 2.0 * Flash::FIELD_MARGIN) * scale,
					Flash::Number(a_cell, "textHeight"sv, nan));
				Flash::Set(a_cell, "text"sv, Value(""));
				return false;
			}
			Flash::Set(a_cell, "textColor"sv, Value(Flash::Number(a_field, "textColor"sv)));
			if (const auto tint = Flash::TintOf(a_field)) {
				Flash::SetTint(a_cell, *tint);
			}
			return true;
		}

		// -------------------------------------------------------------------
		// What the last write left
		// -------------------------------------------------------------------

		// A CND row as a render finds it, for the best try it should show.
		struct RowState
		{
			const Scaleform::GFx::Movie* movie = nullptr;  // only compared, never read
			std::string                  best;             // the field's first try, the rate with its sign
			std::string                  number;           // the number field's text
			std::string                  beside;           // the cell's text, trimmed
			double                       room = 0.0;       // the cell's width, the room WriteInCell measures
			double                       x = 0.0;          // the number field's place and width
			double                       width = 0.0;

			bool operator==(const RowState&) const = default;
		};

		RowState StateOf(const Scaleform::GFx::Movie& a_movie, const Value& a_field, const Value& a_cell, const std::string& a_best)
		{
			return { &a_movie, a_best, Flash::String(a_field, "text"sv),
				a_cell.IsDisplayObject() ? std::string{ IniText::Trimmed(Flash::String(a_cell, "text"sv)) } : std::string{},
				Flash::Number(a_cell, "width"sv), Flash::Number(a_field, "x"sv), Flash::Number(a_field, "width"sv) };
		}

		// The row as the last write that measured left it. Only 1 card's
		// menu is open at a time, so 1 is enough, and a row it does not
		// match is only measured again. WatchCard clears it when a card's
		// movie loads, so it never outlives its movie. Behind a lock, as
		// Raise.cpp keeps its own.
		std::mutex g_lastLock;
		RowState   g_last;
	}

	void ForgetWearRate()
	{
		const std::scoped_lock lock{ g_lastLock };
		g_last = {};
	}

	// -------------------------------------------------------------------
	// The rate of an item
	// -------------------------------------------------------------------

	std::optional<float> WearRateOf(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack)
	{
		const auto* object = a_item.object;
		if (!object) {
			return std::nullopt;
		}
		const auto*          extra = a_stack ? a_stack->extra.get() : nullptr;
		std::optional<float> rate;
		if (const auto* weapon = object->As<RE::TESObjectWEAP>()) {
			rate = WeaponWear::WearShare(*weapon, extra);
		} else if (const auto* armor = object->As<RE::TESObjectARMO>()) {
			rate = ArmorWear::WearShare(*armor, extra);
		} else {
			rate = FoodSpoil::SpoilShare(*object);
		}
		return rate && std::isfinite(*rate) ? rate : std::nullopt;
	}

	// -------------------------------------------------------------------
	// Writing it
	// -------------------------------------------------------------------

	// A render after a write finds the best try it wrote, the rate with its
	// sign, and leaves it. A later try, the rate without its sign or no rate
	// at all, is left only while the row reads as the last write left it for
	// the same best try: the same card, the same number, the same cell text
	// and width, and the field at the same place and width. So a row with
	// no room is measured again when its number, its cell or its box
	// changes, or its movie loads again. A change elsewhere on the row alone,
	// such as the label's size, is not looked for. A rate written for
	// another copy at an earlier render is written again. Each trace line
	// comes once for the same card, since the listener runs at every redraw.
	void WriteWearRate(Scaleform::GFx::Movie& a_movie, std::string_view a_movieFile, Value& a_row, std::optional<float> a_rate,
		std::optional<float> a_compared)
	{
		if (!a_rate) {
			Roles::Noted(a_movie, "CND row's wear rate"sv, "CND shows alone"sv);
			return;
		}
		auto  field = Roles::Card::NumberField(a_row);
		Value shown;
		if (!field.IsDisplayObject() || !field.GetMember("text", &shown) || !shown.IsString()) {
			return;
		}

		const auto  tries = FieldTries(*a_rate, a_compared);
		std::string printed{ shown.GetString() };
		const auto  at = printed.find(WEAR_RATE_START);
		if (at != std::string::npos && std::string_view{ printed }.substr(at + 1) == tries.front()) {
			return;
		}
		auto cell = Roles::Card::CellBeside(a_row);
		const auto now = StateOf(a_movie, field, cell, tries.front());
		{
			const std::scoped_lock lock{ g_lastLock };
			if (now == g_last) {
				return;
			}
		}
		const auto remember = [&] {
			const std::scoped_lock lock{ g_lastLock };
			g_last = StateOf(a_movie, field, cell, tries.front());
		};

		// The field's old rate, and its sign with it, is cut off before the
		// new one goes in.
		if (at != std::string::npos) {
			printed.resize(at);
			Flash::Set(field, "text"sv, Value(printed.c_str()));
		}

		// A cell beside that shows something of its own, a difference to the
		// item compared, leaves no room for the rate. A cell of spaces shows
		// nothing and is left as it is, as on DEF_UI's workbench card.
		const auto& beside = now.beside;
		const auto cellTries = CellTries(*a_rate, a_compared);
		if (beside == cellTries.front()) {
			return;
		}
		const auto movie = MovieName(a_movieFile);
		if (!beside.empty() && !IsWearRateText(beside)) {
			TraceLog::First("menu", "{:s} CND row has no room for \"{:s}\" and reads \"{:s}\"", movie, tries.front(), printed);
			return;
		}

		for (std::size_t i = 0; i < tries.size(); i++) {
			const auto text = printed + " " + tries[i];
			if (const auto wider = WriteInField(a_movie, a_row, field, printed, text)) {
				if (!beside.empty()) {
					Flash::Set(cell, "text"sv, Value(""));
				}
				// A rate that shows without its sign names the sign's try.
				if (i == 0) {
					TraceLog::First("menu", "{:s} CND row reads \"{:s}\", {:.0f} wider to the left", movie, text, *wider);
				} else {
					TraceLog::First("menu", "{:s} CND row has no room for \"{:s}\" and reads \"{:s}\"", movie, tries.front(), text);
				}
				remember();
				return;
			}
		}
		if (cell.IsDisplayObject()) {
			// Only the last try's reason is traced, the rate without a sign,
			// as it says why the row shows no rate at all.
			std::string why;
			for (const auto& rate : cellTries) {
				if (WriteInCell(a_movie, field, cell, rate, why)) {
					TraceLog::First("menu", "{:s} CND row reads \"{:s}\" with \"{:s}\" in the cell beside it", movie, printed, rate);
					remember();
					return;
				}
			}
			TraceLog::First("menu", "{:s} CND row's cell beside the number {:s}", movie, why);
		}
		if (!beside.empty()) {
			Flash::Set(cell, "text"sv, Value(""));
		}
		remember();
		TraceLog::First("menu", "{:s} CND row has no room for \"{:s}\" and reads \"{:s}\"", movie, tries.front(), printed);
	}
}
