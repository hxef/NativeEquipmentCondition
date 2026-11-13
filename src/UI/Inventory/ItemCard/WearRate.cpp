#include "UI/Inventory/ItemCard/Cards.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/WeaponWear/WeaponWear.h"
#include "Core/IniText.h"
#include "Core/TraceLog.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "UI/Flash.h"
#include "UI/Roles/Card.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>

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
		// number from 10 up, (x18) for 17.6, else as WearRateText writes it.
		std::string CellWearRateText(float a_rate)
		{
			return a_rate < 9.95F ? WearRateText(a_rate) : std::format("(x{:.0f})", a_rate);
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

	// A render after a write finds the text it wrote and leaves it. A rate
	// written for another copy at an earlier render is written again. Each
	// trace line comes once for the same card, since the listener runs at
	// every redraw.
	void WriteWearRate(Scaleform::GFx::Movie& a_movie, std::string_view a_movieFile, Value& a_row, std::optional<float> a_rate)
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

		const auto  rate = WearRateText(*a_rate);
		std::string printed{ shown.GetString() };
		if (const auto at = printed.find(WEAR_RATE_START); at != std::string::npos) {
			if (std::string_view{ printed }.substr(at + 1) == rate) {
				return;
			}
			printed.resize(at);
			Flash::Set(field, "text"sv, Value(printed.c_str()));
		}

		// A cell beside that shows something of its own, a difference to the
		// item compared, leaves no room for the rate. A cell of spaces shows
		// nothing and is left as it is, as on DEF_UI's workbench card.
		auto       cell = Roles::Card::CellBeside(a_row);
		const auto beside = cell.IsDisplayObject() ? std::string{ IniText::Trimmed(Flash::String(cell, "text"sv)) } : std::string{};
		const auto cellWearRate = CellWearRateText(*a_rate);
		if (beside == cellWearRate) {
			return;
		}
		const auto movie = MovieName(a_movieFile);
		if (!beside.empty() && !beside.starts_with("(x")) {
			TraceLog::First("menu", "{:s} CND row has no room for \"{:s}\" and reads \"{:s}\"", movie, rate, printed);
			return;
		}

		const auto text = printed + " " + rate;
		if (const auto wider = WriteInField(a_movie, a_row, field, printed, text)) {
			if (!beside.empty()) {
				Flash::Set(cell, "text"sv, Value(""));
			}
			TraceLog::First("menu", "{:s} CND row reads \"{:s}\", {:.0f} wider to the left", movie, text, *wider);
			return;
		}
		if (cell.IsDisplayObject()) {
			std::string why;
			if (WriteInCell(a_movie, field, cell, cellWearRate, why)) {
				TraceLog::First("menu", "{:s} CND row reads \"{:s}\" with \"{:s}\" in the cell beside it", movie, printed, cellWearRate);
				return;
			}
			TraceLog::First("menu", "{:s} CND row's cell beside the number {:s}", movie, why);
		}
		if (!beside.empty()) {
			Flash::Set(cell, "text"sv, Value(""));
		}
		TraceLog::First("menu", "{:s} CND row has no room for \"{:s}\" and reads \"{:s}\"", movie, rate, printed);
	}
}
