#include "UI/Inventory/ItemCard/WearRateForms.h"

#include "Core/IniText.h"
#include "UI/Flash.h"
#include "UI/Roles/Card.h"

#include <algorithm>
#include <cctype>
#include <charconv>
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
		// Measuring
		// -------------------------------------------------------------------

		// Room left over in a field that holds the text at its natural size,
		// so rounding never tips it into shrinking.
		constexpr double SLACK = 1.0;

		// How far 2 measures may differ and still be the same size.
		constexpr double SAME = 0.5;

		// The least share of its natural size the movie's shrink may draw
		// the number and its rate at. On vanilla's row under one wider
		// font, 89.4% (x16.9) shows at 95% and a rate of 1000 at 91%.
		constexpr double SMALLEST = 0.9;

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
			// The engine's fonts live in its font library, which is what
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
		// before it. True when it fits a_width and shows from a_least to
		// a_most high, so the field cut none of it, did not wrap it and
		// shrank it no further than a_least. A field that shrinks its text
		// reads back the height and the width it drew.
		bool WriteShown(Value& a_field, const Value& a_format, const std::string& a_text, double a_least, double a_most, double a_width)
		{
			if (!Flash::Set(a_field, "text"sv, Value(a_text.c_str()))) {
				return false;
			}
			Flash::Call(a_field, "setTextFormat", std::span{ &a_format, 1 });
			const auto nan = std::numeric_limits<double>::quiet_NaN();
			const auto height = Flash::Number(a_field, "textHeight"sv, nan);
			const auto width = (Flash::Number(a_field, "textWidth"sv, nan) + 2.0 * Flash::FIELD_MARGIN) * Flash::Number(a_field, "scaleX"sv, 1.0);
			return height >= a_least - SAME && height <= a_most + SAME && width <= a_width + SAME;
		}

		// How a field's text looks now, as the movie wrote it.
		struct Look
		{
			std::string text;
			Value       format;
			double      colour;
		};

		// A field's look, or nothing when it has no format or colour to read.
		std::optional<Look> LookOf(Value& a_field)
		{
			const auto format = FormatOf(a_field);
			const auto colour = Flash::Member(a_field, "textColor"sv);
			if (!format || !Flash::IsAnyNumber(colour)) {
				return std::nullopt;
			}
			return Look{ Flash::String(a_field, "text"sv), *format, Flash::AsNumber(colour) };
		}

		// Writes a_text into a_field with a_look. Setting text alone would
		// give it the field's default format, so the format goes on after it,
		// and the colour last, as the movie set it.
		void PutLook(Value& a_field, const std::string& a_text, const Look& a_look)
		{
			Flash::Set(a_field, "text"sv, Value(a_text.c_str()));
			Flash::Call(a_field, "setTextFormat", std::span{ &a_look.format, 1 });
			Flash::Set(a_field, "textColor"sv, Value(a_look.colour));
		}
	}

	// -------------------------------------------------------------------
	// The writes
	// -------------------------------------------------------------------

	std::optional<FieldWrite> WriteInField(Scaleform::GFx::Movie& a_movie, Value& a_row, Value& a_field, const std::string& a_printed,
		const std::string& a_text, Fit a_fit)
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

		// Only kShrunk takes a text that at its natural size would come
		// nearer to the label than LABEL_GAP.
		const auto room = (natural->width + 2.0 * Flash::FIELD_MARGIN + SLACK) * scale;
		const auto tooNear = x + width - room < labelEnd + LABEL_GAP;
		const auto shrink = a_fit == Fit::kShrunk;
		if (tooNear && !shrink) {
			return std::nullopt;
		}
		// Such a text gets a field that starts LABEL_GAP right of the label,
		// and the movie scales a text too wide for its field to fill it from
		// margin to margin. The field never gets narrower. A field an
		// earlier write already widened this far may read back up to SAME
		// short, which still counts.
		const auto reach = x + width - (labelEnd + LABEL_GAP);
		const auto need = std::max(width, tooNear ? reach : room);
		const auto share = tooNear ? (need / scale - 2.0 * Flash::FIELD_MARGIN) / natural->width : 1.0;
		const auto left = x + width - need;
		if ((tooNear && reach < width - SAME) || share < SMALLEST || (need > width && Roles::Card::DrawsAcross(a_row, left, x))) {
			return std::nullopt;
		}

		Flash::Set(a_field, "width"sv, Value(need));
		Flash::Set(a_field, "x"sv, Value(left));
		// kAsBefore holds the text to the height the number had. kShrunk
		// takes it from SMALLEST of its natural height up to its natural
		// height, or the number's own where that was taller.
		const auto least = shrink ? natural->height * SMALLEST : height;
		const auto most = shrink ? std::max(height, natural->height) : height;
		if (WriteShown(a_field, *format, a_text, least, most, need)) {
			const auto shown = Flash::Number(a_field, "textHeight"sv, natural->height);
			return FieldWrite{ need - width, shrink ? std::min(1.0, shown / natural->height) : 1.0 };
		}
		// The field as it was. Its box goes back first, so the movie's
		// shrink sizes the number for it again.
		Flash::Set(a_field, "width"sv, Value(width));
		Flash::Set(a_field, "x"sv, Value(x));
		WriteShown(a_field, *format, a_printed, height, height, width);
		return std::nullopt;
	}

	bool WriteInCell(Scaleform::GFx::Movie& a_movie, Value& a_field, Value& a_cell, const std::string& a_rate, bool a_keepColour,
		std::string& a_why)
	{
		auto format = FormatOf(a_field);
		if (!format) {
			a_why = "gets no text format, as the number has none";
			return false;
		}
		if (const auto align = Flash::Member(Flash::Member(a_cell, "defaultTextFormat"sv), "align"sv); align.IsString()) {
			Flash::Set(*format, "align"sv, align);
		}
		// A cell whose look cannot be read takes the number's colour.
		const auto kept = a_keepColour ? LookOf(a_cell) : std::nullopt;
		if (kept) {
			Flash::Set(*format, "color"sv, Value(kept->colour));
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
		if (!WriteShown(a_cell, *format, a_rate, natural->height, natural->height, width)) {
			const auto nan = std::numeric_limits<double>::quiet_NaN();
			a_why = std::format("is {:.1f} wide, and \"{:s}\" needs {:.1f} at {:.1f} high, but it drew {:.1f} wide at {:.1f} high", width,
				a_rate, need, natural->height, (Flash::Number(a_cell, "textWidth"sv, nan) + 2.0 * Flash::FIELD_MARGIN) * scale,
				Flash::Number(a_cell, "textHeight"sv, nan));
			if (kept) {
				PutLook(a_cell, kept->text, *kept);
			} else {
				Flash::Set(a_cell, "text"sv, Value(""));
			}
			return false;
		}
		if (kept) {
			// The cell's own tint stays as the movie left it.
			Flash::Set(a_cell, "textColor"sv, Value(kept->colour));
			return true;
		}
		Flash::Set(a_cell, "textColor"sv, Value(Flash::Number(a_field, "textColor"sv)));
		if (const auto tint = Flash::TintOf(a_field)) {
			Flash::SetTint(a_cell, *tint);
		}
		return true;
	}

	std::optional<double> FontSize(Value& a_field)
	{
		const auto format = FormatOf(a_field);
		const auto size = format ? Flash::Number(*format, "size"sv, std::numeric_limits<double>::quiet_NaN()) : 0.0;
		return std::isfinite(size) && size > 0.0 ? std::optional{ size } : std::nullopt;
	}

	std::optional<double> NaturalWidth(Scaleform::GFx::Movie& a_movie, Value& a_field, const std::string& a_text)
	{
		const auto format = FormatOf(a_field);
		const auto natural = format ? NaturalSize(a_movie, *format, a_text) : std::nullopt;
		return natural ? std::optional{ natural->width } : std::nullopt;
	}

	bool WriteKeepingLook(Value& a_field, const std::string& a_text)
	{
		const auto look = LookOf(a_field);
		if (!look) {
			return false;
		}
		PutLook(a_field, a_text, *look);
		return true;
	}

	// -------------------------------------------------------------------
	// What the cell beside the number shows
	// -------------------------------------------------------------------

	bool IsWearRateText(std::string_view a_text)
	{
		if (a_text.starts_with("(x"sv)) {
			return true;
		}
		return a_text.size() > 1 && a_text[0] == 'x' && (a_text[1] == '.' || std::isdigit(static_cast<unsigned char>(a_text[1])));
	}

	std::optional<std::string> TurnedChange(std::string_view a_text, char a_sign)
	{
		const auto shown = IniText::Trimmed(a_text);
		const auto bracketed = shown.size() > 1 && shown.front() == '(' && shown.back() == ')';
		const auto change = bracketed ? shown.substr(1, shown.size() - 2) : shown;
		if (change.size() < 2 || (change[0] != '+' && change[0] != '-') || change[0] == a_sign) {
			return std::nullopt;
		}
		// After the sign: digits, then at most a point and more digits.
		const auto digits = [](std::string_view a_digits) {
			return !a_digits.empty() && std::ranges::all_of(a_digits, [](char a_char) { return std::isdigit(static_cast<unsigned char>(a_char)) != 0; });
		};
		const auto number = change.substr(1);
		const auto point = number.find('.');
		const auto isNumber = point == std::string_view::npos ? digits(number) : (digits(number.substr(0, point)) && digits(number.substr(point + 1)));
		if (!isNumber) {
			return std::nullopt;
		}
		std::string turned{ a_text };
		turned[static_cast<std::size_t>(change.data() - a_text.data())] = a_sign;
		return turned;
	}

	bool ShowsFedChange(std::string_view a_text, float a_change)
	{
		auto shown = IniText::Trimmed(a_text);
		if (shown.empty() || IsWearRateText(shown) || shown.find_first_not_of("+- "sv) == std::string_view::npos) {
			return true;
		}
		if (shown.size() > 1 && shown.front() == '(' && shown.back() == ')') {
			shown = shown.substr(1, shown.size() - 2);
		}
		if (!shown.empty() && (shown.front() == '+' || shown.front() == '-')) {
			shown = shown.substr(1);
		}
		float      number = 0.0F;
		const auto end = shown.data() + shown.size();
		const auto [at, error] = std::from_chars(shown.data(), end, number);
		if (error != std::errc{} || at != end) {
			return false;
		}
		const auto cut = std::trunc(std::abs(a_change) * 10.0F) / 10.0F;
		return std::abs(number - cut) < 0.001F;
	}
}
