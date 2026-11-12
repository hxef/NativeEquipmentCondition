#include "UI/Repair/VendorRepair/SignLine.h"

#include "Core/Text/Text.h"
#include "UI/Roles/Roles.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <utility>

namespace VendorRepair::Sign
{
	namespace
	{
		using Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// The text
		// -------------------------------------------------------------------

		constexpr const char* LINE_NAME = "NEC_TraderSignLine_tf";

		// The card's own fonts: words as its row names, numbers as its
		// values. Each language maps them to its own letters.
		constexpr const char* WORD_FONT = "$MAIN_Font";
		constexpr const char* NUMBER_FONT = "$MAIN_Font_Bold";

		// The line's size as a share of the title's, and the smallest it
		// takes, in the side's units: 15 beside vanilla's title of 25.
		constexpr double SHARE = 0.6;
		constexpr double LEAST = 15.0;

		constexpr Trade TRADES[]{ Trade::kWeapons, Trade::kArmor, Trade::kClothing };

		// The sign's parts as htmlText: the title, then "Weapons 100%" per
		// kind with the number in the card's bold, or "No repairs" alone.
		[[nodiscard]] std::vector<std::string> Parts(const Ceilings& a_ceilings)
		{
			std::vector<std::string> parts;
			for (const auto trade : TRADES) {
				if (const auto level = a_ceilings.Of(trade); level > 0) {
					parts.push_back(std::format(R"({:s} <font face="{:s}">{:d}%</font>)", Flash::HtmlEscaped(Text::SignKind(trade)), NUMBER_FONT, level));
				}
			}
			parts.insert(parts.begin(), Flash::HtmlEscaped(parts.empty() ? Text::SignNone() : Text::SignTitle()));
			return parts;
		}

		// a_parts 2 spaces apart and right aligned, with a 2nd row from
		// a_break on, or all on 1 row for 0. In white, which ColorLine tints.
		[[nodiscard]] std::string Html(const std::vector<std::string>& a_parts, std::size_t a_break, long a_size)
		{
			std::string text;
			for (std::size_t i = 0; i < a_parts.size(); i++) {
				text += i == 0 ? "" : (i == a_break ? "<br>" : "  ");
				text += a_parts[i];
			}
			return std::format(R"(<p align="right"><font face="{:s}" size="{:d}" color="#FFFFFF" letterSpacing="0" kerning="0">{:s}</font></p>)",
				WORD_FONT, a_size, text);
		}

		// -------------------------------------------------------------------
		// The field
		// -------------------------------------------------------------------

		// A text field for the line, sized to its text, which the mouse
		// passes through. Not a display object when the movie refuses a write.
		[[nodiscard]] Value Field(Scaleform::GFx::Movie& a_movie)
		{
			Value field;
			a_movie.CreateObject(&field, "flash.text.TextField");
			// The game's fonts live in its font library, which is what
			// embedFonts asks for.
			const auto made = field.IsDisplayObject() && Flash::Set(field, "name"sv, Value(LINE_NAME)) && Flash::Set(field, "embedFonts"sv, Value(true)) &&
			                  Flash::Set(field, "selectable"sv, Value(false)) && Flash::Set(field, "mouseEnabled"sv, Value(false)) &&
			                  Flash::Set(field, "multiline"sv, Value(true)) && Flash::Set(field, "wordWrap"sv, Value(false)) &&
			                  Flash::Set(field, "autoSize"sv, Value("left"));
			return made ? field : Value();
		}

		// How far the line's bottom sits above the frame's top, in stage px.
		// The frame's top is measured, see Room::top.
		constexpr double CLEAR = 0.75;
	}

	std::optional<Line> LineOf(Scaleform::GFx::Movie& a_movie, Roles::TraderSide::Side& a_side, const Ceilings& a_ceilings)
	{
		auto&      title = a_side.title;
		const auto drawn = Roles::BoundsIn(title, a_side.side);
		const auto own = Roles::BoundsIn(title, title);
		Value      format;
		if (!drawn || !own || !title.IsDisplayObject() || !title.Invoke("getTextFormat", &format) || !format.IsObject()) {
			return std::nullopt;
		}
		const auto scale = (drawn->bottom - drawn->top) / (own->bottom - own->top);
		if (!std::isfinite(scale) || !(scale > 0.0)) {
			return std::nullopt;
		}
		const auto size = std::lround(std::max(LEAST, SHARE * Flash::Number(format, "size"sv) * scale));
		Line       line{ Field(a_movie), {} };
		if (!line.field.IsDisplayObject()) {
			return std::nullopt;
		}
		if (const auto filters = Flash::Member(title, "filters"sv); filters.IsArray()) {
			Flash::Set(line.field, "filters"sv, filters);
		}

		const auto parts = Parts(a_ceilings);
		const auto measured = [&](std::string_view a_said, std::string_view a_beside, std::size_t a_break) -> std::optional<Form> {
			auto       html = Html(parts, a_break, size);
			const auto box = Flash::Set(line.field, "htmlText"sv, Value(html.c_str())) ? Roles::BoundsIn(line.field, line.field) : std::nullopt;
			return box ? std::optional{ Form{ a_said, a_beside, std::move(html), *box } } : std::nullopt;
		};
		const auto wide = [](const Form& a_form) { return a_form.own.right - a_form.own.left; };

		auto one = measured("shows as 1 line above the trader's list"sv, "shows as 1 line left of the trader's list"sv, 0);
		if (!one) {
			return std::nullopt;
		}
		line.forms.push_back(std::move(*one));
		if (parts.size() < 2) {
			return line;
		}
		if (auto over = measured("shows as 2 rows above the trader's list, its title alone on the upper row"sv,
				"shows as 2 rows left of the trader's list, its title alone on the upper row"sv, 1)) {
			line.forms.push_back(std::move(*over));
		}
		// The break that leaves the wider row narrowest, if not the title's.
		std::optional<Form> shared;
		for (std::size_t at = 2; at < parts.size(); at++) {
			if (auto form = measured("shows as 2 rows above the trader's list, its kinds on both rows"sv, "shows as 2 rows left of the trader's list, its kinds on both rows"sv, at);
				form && (!shared || wide(*form) < wide(*shared))) {
				shared = std::move(form);
			}
		}
		if (shared && wide(*shared) < wide(line.forms.back())) {
			line.forms.push_back(std::move(*shared));
		}
		return line;
	}

	void ColorLine(Value& a_line, Roles::TraderSide::Side& a_side)
	{
		constexpr std::uint32_t WHITE = 0xFFFFFF;

		// The colour the title's text holds, as its format reads it when the
		// whole text shares one. textColor reads only the colour new text
		// starts with, which a movie that colours the text itself leaves as
		// it was.
		auto& title = a_side.title;
		Value format;
		if (title.IsDisplayObject()) {
			title.Invoke("getTextFormat", &format);
		}
		const auto held = Flash::Member(format, "color"sv);
		const auto color = static_cast<std::uint32_t>(Flash::IsAnyNumber(held) ? Flash::AsNumber(held) : Flash::Number(title, "textColor"sv, WHITE)) & WHITE;
		const auto part = [&](int a_shift) { return static_cast<double>((color >> a_shift) & 0xFF) / 255.0; };

		// The title shows that colour under its own clip's and its header's,
		// and the line sits beside the header, so it takes all 3.
		const auto tint = Flash::TintOf(Flash::Member(title, "parent"sv))
		                      .value_or(Flash::Tint{})
		                      .Over(Flash::TintOf(title).value_or(Flash::Tint{}))
		                      .Over(Flash::Tint{ part(16), part(8), part(0) });
		if (const auto now = Flash::TintOf(a_line); !now || !now->Near(tint)) {
			Flash::SetTint(a_line, tint);
		}
	}

	Spot SpotOf(const Form& a_form, const Roles::TraderSide::Room& a_room, bool a_beside)
	{
		const auto bottom = a_room.top - CLEAR * a_room.unit;
		const auto right = a_beside ? a_room.bracket.left - GAP * a_room.unit : a_room.bracket.right;
		const auto margin = Flash::FIELD_MARGIN;
		Spot       spot;
		spot.x = right - a_form.own.right;
		spot.y = bottom - a_form.own.bottom;
		spot.box = Flash::Box{ spot.x + a_form.own.left + margin, spot.y + a_form.own.top + margin, right - margin, bottom - margin };
		return spot;
	}

	TitleRow RowOf(Roles::TraderSide::Side& a_side, bool a_each)
	{
		const auto header = Flash::Member(a_side.title, "parent"sv);
		TitleRow   row{ Flash::String(a_side.title, "text"sv), Flash::Opacity(header), Flash::Number(header, "width"sv), Flash::Number(a_side.side, "numChildren"sv),
			Flash::Number(a_side.title, "textColor"sv), {} };
		if (a_each) {
			auto& each = row.each.emplace();
			for (const auto& child : Flash::Children(a_side.side)) {
				each.push_back(Flash::Opacity(child) > 0.0);
			}
		}
		return row;
	}
}
