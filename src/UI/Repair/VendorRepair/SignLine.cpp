#include "UI/Repair/VendorRepair/SignLine.h"

#include "Core/Text/Text.h"
#include "UI/Roles/Roles.h"

#include <algorithm>
#include <array>
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
		constexpr const char* FRAME_NAME = "NEC_TraderSignLineFrame";

		// The line's size as a share of the title's, and the smallest it
		// takes, in the side's units: 15 beside vanilla's title of 25.
		constexpr double SHARE = 0.6;
		constexpr double LEAST = 15.0;

		// The sign's parts as htmlText: the title, then "Weapons 100%" per
		// kind with the number in bold, or "No repairs" alone. With a_short
		// the kinds of 1 level share 1 number, "Wpn/Clo 100%", where the
		// first of them stands.
		[[nodiscard]] std::vector<std::string> Parts(const Ceilings& a_ceilings, bool a_short)
		{
			struct Group
			{
				std::uint32_t level;
				std::string   names;
			};
			std::vector<Group> groups;
			for (const auto trade : TRADES) {
				const auto level = a_ceilings.Of(trade);
				if (level == 0) {
					continue;
				}
				const auto name = Flash::HtmlEscaped(a_short ? Text::SignKindShort(trade) : Text::SignKind(trade));
				if (const auto same = std::ranges::find(groups, level, &Group::level); a_short && same != groups.end()) {
					same->names += "/" + name;
				} else {
					groups.push_back({ level, name });
				}
			}
			std::vector<std::string> parts{ Flash::HtmlEscaped(groups.empty() ? Text::SignNone() : Text::SignTitle()) };
			for (const auto& [level, names] : groups) {
				parts.push_back(std::format(R"({:s} <font face="{:s}">{:d}%</font>)", names, NUMBER_FONT, level));
			}
			return parts;
		}

		// a_parts 2 spaces apart, with a 2nd row from a_break on, or all on 1
		// row for 0. 1 row is right aligned, 2 rows start at the left, so the
		// title starts over the kinds. In white, which ColorLine tints.
		[[nodiscard]] std::string Html(const std::vector<std::string>& a_parts, std::size_t a_break, long a_size)
		{
			std::string text;
			for (std::size_t i = 0; i < a_parts.size(); i++) {
				text += i == 0 ? "" : (i == a_break ? "<br>" : "  ");
				text += a_parts[i];
			}
			return std::format(R"(<p align="{:s}"><font face="{:s}" size="{:d}" color="#FFFFFF" letterSpacing="0" kerning="0">{:s}</font></p>)",
				a_break == 0 ? "right" : "left", WORD_FONT, a_size, text);
		}

		// How far the line's bottom sits above the frame's top, in stage px.
		// The frame's top is measured, see Room::top.
		constexpr double CLEAR = 0.75;

		// -------------------------------------------------------------------
		// The frame around the 1 line form
		// -------------------------------------------------------------------

		// From the letters to the frame's ends, and from the line's top and
		// bottom to its 2 lines, as shares of the line's size: 8 and 2.25
		// beside a title of 25, the approved look.
		constexpr double FRAME_IN = 0.53;
		constexpr double FRAME_PAD = 0.15;

		// The 2 lines of the frame around a_one's text, at a_size, in the
		// field's units.
		[[nodiscard]] Flash::Box FrameLines(const Form& a_one, double a_size)
		{
			const auto margin = Flash::FIELD_MARGIN;
			const auto in = FRAME_IN * a_size;
			const auto pad = FRAME_PAD * a_size;
			return { a_one.own.left + margin - in, a_one.own.top + margin - pad, a_one.own.right - margin + in, a_one.own.bottom - margin + pad };
		}

		// The box a frame's lines draw, with the stroke past them.
		[[nodiscard]] Flash::Box Drawn(const Flash::Box& a_lines)
		{
			return { a_lines.left - STROKE, a_lines.top - STROKE, a_lines.right + STROKE, a_lines.bottom + STROKE };
		}
	}

	// -------------------------------------------------------------------
	// What the line and the panel share
	// -------------------------------------------------------------------

	std::optional<double> TitleSize(Roles::TraderSide::Side& a_side)
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
		return Flash::Number(format, "size"sv) * scale;
	}

	Value FieldOf(Scaleform::GFx::Movie& a_movie, Roles::TraderSide::Side& a_side, const char* a_name)
	{
		Value field;
		a_movie.CreateObject(&field, "flash.text.TextField");
		// The engine's fonts live in its font library, which is what
		// embedFonts asks for.
		const auto made = field.IsDisplayObject() && Flash::Set(field, "name"sv, Value(a_name)) && Flash::Set(field, "embedFonts"sv, Value(true)) &&
		                  Flash::Set(field, "selectable"sv, Value(false)) && Flash::Set(field, "mouseEnabled"sv, Value(false)) &&
		                  Flash::Set(field, "multiline"sv, Value(true)) && Flash::Set(field, "wordWrap"sv, Value(false)) &&
		                  Flash::Set(field, "autoSize"sv, Value("left"));
		if (!made) {
			return {};
		}
		if (const auto filters = Flash::Member(a_side.title, "filters"sv); filters.IsArray()) {
			Flash::Set(field, "filters"sv, filters);
		}
		return field;
	}

	Value ShapeOf(Scaleform::GFx::Movie& a_movie, const char* a_name)
	{
		Value shape;
		a_movie.CreateObject(&shape, "flash.display.Shape");
		return shape.IsDisplayObject() && Flash::Set(shape, "name"sv, Value(a_name)) ? shape : Value();
	}

	bool DrawFrame(Value& a_shape, const Flash::Box& a_frame, std::optional<std::pair<double, double>> a_gap)
	{
		Value      graphics;
		const auto to = [&](const char* a_how, double a_x, double a_y) {
			return graphics.Invoke(a_how, std::array{ Value(a_x), Value(a_y) });
		};
		const auto [left, top, right, bottom] = a_frame;
		// lineStyle(2, 0xFFFFFF, 1, true, LineScaleMode.NONE), vanilla's
		// call for its own lines, so the side's colour paints them.
		const auto styled = a_shape.GetMember("graphics"sv, &graphics) && graphics.IsObject() &&
		                    graphics.Invoke("lineStyle", std::array{ Value(2.0), Value(0xFFFFFF), Value(1.0), Value(true), Value("none") });
		if (!styled || !to("moveTo", left, top + HOOK) || !to("lineTo", left, top)) {
			return false;
		}
		if (a_gap && !(to("lineTo", a_gap->first, top) && to("moveTo", a_gap->second, top))) {
			return false;
		}
		return to("lineTo", right, top) && to("lineTo", right, top + HOOK) && to("moveTo", left, bottom - HOOK) && to("lineTo", left, bottom) &&
		       to("lineTo", right, bottom) && to("lineTo", right, bottom - HOOK);
	}

	bool Reframe(Value& a_shape, const Form& a_form)
	{
		Value graphics;
		return a_form.frame && a_shape.GetMember("graphics"sv, &graphics) && graphics.IsObject() && graphics.Invoke("clear") &&
		       DrawFrame(a_shape, *a_form.frame);
	}

	double RightEnd(const Roles::TraderSide::Room& a_room)
	{
		return a_room.lines ? a_room.lines->right : a_room.bracket.right;
	}

	// -------------------------------------------------------------------
	// The line
	// -------------------------------------------------------------------

	std::optional<Line> LineOf(Scaleform::GFx::Movie& a_movie, Roles::TraderSide::Side& a_side, const Ceilings& a_ceilings)
	{
		const auto title = TitleSize(a_side);
		if (!title) {
			return std::nullopt;
		}
		const auto size = std::lround(std::max(LEAST, SHARE * *title));
		Line       line{ FieldOf(a_movie, a_side, LINE_NAME), {}, {} };
		if (!line.field.IsDisplayObject()) {
			return std::nullopt;
		}

		// A 1 line form also gets its frame's lines.
		const auto measured = [&](const std::vector<std::string>& a_parts, std::size_t a_break, std::string_view a_said, std::string_view a_saidLeft,
								  std::string_view a_saidFramed = {}) -> std::optional<Form> {
			auto       html = Html(a_parts, a_break, size);
			const auto box = Flash::Set(line.field, "htmlText"sv, Value(html.c_str())) ? Roles::BoundsIn(line.field, line.field) : std::nullopt;
			if (!box) {
				return std::nullopt;
			}
			Form form{ a_said, a_saidLeft, a_saidFramed, std::move(html), *box, std::nullopt };
			if (a_break == 0) {
				form.frame = FrameLines(form, static_cast<double>(size));
			}
			return form;
		};
		const auto wide = [](const Form& a_form) { return a_form.own.right - a_form.own.left; };

		const auto parts = Parts(a_ceilings, false);
		auto       one = measured(parts, 0, "shows as 1 line above the trader's list"sv, "shows as 1 line left of the trader's frame"sv,
			"shows as 1 line in a frame above the trader's list"sv);
		if (!one) {
			return std::nullopt;
		}
		line.forms.push_back(std::move(*one));
		if (auto shape = ShapeOf(a_movie, FRAME_NAME); shape.IsDisplayObject() && DrawFrame(shape, *line.forms.front().frame)) {
			line.frame = shape;
		}
		if (parts.size() < 2) {
			return line;
		}
		if (auto brief = measured(Parts(a_ceilings, true), 0, "shows as 1 line above the trader's list, with short kind names"sv,
				"shows as 1 line left of the trader's frame, with short kind names"sv, "shows as 1 line in a frame above the trader's list, with short kind names"sv);
			brief && wide(*brief) < wide(line.forms.front())) {
			line.forms.push_back(std::move(*brief));
		}

		// 2 rows keep the full names: the title alone on the upper row, then
		// the break that leaves the wider row narrowest, if narrower still.
		auto over = measured(parts, 1, "shows as 2 rows above the trader's list, its title alone on the upper row"sv,
			"shows as 2 rows left of the trader's frame, its title alone on the upper row"sv);
		std::optional<Form> shared;
		for (std::size_t at = 2; at < parts.size(); at++) {
			if (auto form = measured(parts, at, "shows as 2 rows above the trader's list, its kinds on both rows"sv, "shows as 2 rows left of the trader's frame, its kinds on both rows"sv);
				form && (!shared || wide(*form) < wide(*shared))) {
				shared = std::move(form);
			}
		}
		const auto least = wide(over ? *over : line.forms.front());
		if (over) {
			line.forms.push_back(std::move(*over));
		}
		if (shared && wide(*shared) < least) {
			line.forms.push_back(std::move(*shared));
		}
		return line;
	}

	void ColorLine(Value& a_clip, Roles::TraderSide::Side& a_side)
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

		// The title shows that colour under its own clip's and its header's,
		// and the sign sits beside the header, so it takes all 3.
		const auto tint = Flash::TintOf(Flash::Member(title, "parent"sv))
		                      .value_or(Flash::Tint{})
		                      .Over(Flash::TintOf(title).value_or(Flash::Tint{}))
		                      .Over(Flash::ColorTint(color));
		if (const auto now = Flash::TintOf(a_clip); !now || !now->Near(tint)) {
			Flash::SetTint(a_clip, tint);
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

	Spot SpotFor(const Forms& a_line, const Pick& a_pick, const Roles::TraderSide::Room& a_room)
	{
		const auto& form = a_line.forms[a_pick.form];
		return a_pick.framed && form.frame ? FramedOf(Drawn(*form.frame), a_room) : SpotOf(form, a_room, a_pick.beside);
	}

	std::optional<Pick> PickOf(const Forms& a_line, const std::optional<Pick>& a_held, bool a_framable, Roles::TraderSide::Side& a_side,
		Roles::TraderSide::Room& a_room, bool& a_around)
	{
		const auto fits = [&](const Pick& a_pick) {
			if (a_pick.form >= a_line.forms.size() || (a_pick.framed && (!a_framable || !a_line.forms[a_pick.form].frame))) {
				return false;
			}
			if (a_pick.beside && !a_around) {
				Roles::TraderSide::AroundOf(a_side, a_room);
				a_around = true;
			}
			return a_room.Fits(SpotFor(a_line, a_pick, a_room).box, GAP, a_pick.beside ? GAP_UNDER_LEFT : GAP, a_pick.beside);
		};
		if (a_held && fits(*a_held)) {
			return a_held;
		}
		// Every try in order: above the list each form, a 1 line form in its
		// frame before it plain, then each form left of the frame.
		for (const auto beside : { false, true }) {
			for (std::size_t i = 0; i < a_line.forms.size(); i++) {
				if (const Pick framed{ i, false, true }; !beside && fits(framed)) {
					return framed;
				}
				if (const Pick plain{ i, beside, false }; fits(plain)) {
					return plain;
				}
			}
		}
		return std::nullopt;
	}

	Spot FramedOf(const Flash::Box& a_frame, const Roles::TraderSide::Room& a_room)
	{
		Spot spot;
		spot.x = RightEnd(a_room) - a_frame.right;
		spot.y = a_room.top - GAP * a_room.unit - a_frame.bottom;
		spot.box = a_frame.Moved(spot.x, spot.y);
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
