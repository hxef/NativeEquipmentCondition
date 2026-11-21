#include "UI/Repair/VendorRepair/SignBlock.h"

#include "Core/Text/Text.h"
#include "UI/Flash.h"
#include "UI/Repair/VendorRepair/SignLine.h"
#include "UI/Roles/Card.h"
#include "UI/Roles/Roles.h"

#include <REX/CONVERT.h>
#include <REX/W32/KERNEL32.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace VendorRepair::Sign
{
	namespace
	{
		using Scaleform::GFx::Value;
		using Flash::Box;

		// -------------------------------------------------------------------
		// Names and sizes
		// -------------------------------------------------------------------

		constexpr const char* CLIP_CLASS = "flash.display.Sprite";

		// No name holds "ItemCard", so nothing that finds the card by name
		// finds the sign.
		constexpr const char* SIGN_NAME = "NEC_TraderSign_mc";
		constexpr const char* BACK_NAME = "NEC_TraderSignBack_mc";
		constexpr const char* FILL_NAME = "NEC_TraderSignBackFill";
		constexpr const char* LINES_NAME = "NEC_TraderSignLines";
		constexpr const char* TITLE_NAME = "NEC_TraderSignTitle_mc";

		struct Kind
		{
			Trade       trade;
			const char* name;
		};

		constexpr Kind KINDS[]{
			{ Trade::kWeapons, "NEC_TraderSignWeapons_mc" },
			{ Trade::kArmor, "NEC_TraderSignArmor_mc" },
			{ Trade::kClothing, "NEC_TraderSignClothing_mc" },
		};

		// The panel's size as a share of the title's, 20 beside vanilla's
		// title of 25, the size of the approved look's rows.
		constexpr double SHARE = 0.78;
		constexpr double LEAST = 15.0;

		// The rest as shares of the panel's size, from the approved look:
		// where the title's letters start from the left frame end, the room
		// the top line leaves each side of them, and how long it runs on
		// after them at least. That gives the look's 169 wide panel in
		// English, and a longer title makes it wider.
		constexpr double TITLE_IN = 0.75;
		constexpr double TITLE_GAP = 0.3;
		constexpr double RIGHT_STUB = 1.4;

		// A row's letters from the frame's ends, and at least this much
		// between a word and its percent.
		constexpr double ROW_IN = 0.4;
		constexpr double COLUMN_GAP = 1.0;

		// From the top line to the first row's box, and from the last row's
		// box to the bottom line. Rows follow box on box, so no 2 meet.
		constexpr double ROW_PAD = 0.1;
		constexpr double BOTTOM_PAD = 0.05;

		// -------------------------------------------------------------------
		// The parts
		// -------------------------------------------------------------------

		// The text in capitals, each letter raised on its own with no
		// language's rules, as the list's title shows its name. Text that
		// will not convert stays as it is.
		[[nodiscard]] std::string Capitals(const std::string& a_text)
		{
			std::wstring wide;
			if (!REX::UTF8_TO_UTF16(a_text, wide) || wide.empty()) {
				return a_text;
			}
			const auto   size = static_cast<std::int32_t>(wide.size());
			std::wstring capitals(wide.size(), L'\0');
			const auto   written = REX::W32::LCMapStringEx(REX::W32::LOCALE_NAME_INVARIANT, REX::W32::LCMAP_UPPERCASE, wide.data(), size, capitals.data(), size, nullptr, nullptr, 0);
			std::string  out;
			return written == size && REX::UTF16_TO_UTF8(capitals, out) ? out : a_text;
		}

		// a_text in a_font at a_size, in white, which ColorLine tints.
		[[nodiscard]] std::string Html(const std::string& a_text, const char* a_font, long a_size)
		{
			return std::format(R"(<font face="{:s}" size="{:d}" color="#FFFFFF" letterSpacing="0" kerning="0">{:s}</font>)", a_font, a_size,
				Flash::HtmlEscaped(a_text));
		}

		// A clip named a_name the mouse passes through.
		[[nodiscard]] Value Clip(Scaleform::GFx::Movie& a_movie, const char* a_name)
		{
			Value clip;
			a_movie.CreateObject(&clip, CLIP_CLASS);
			const auto made = clip.IsDisplayObject() && Flash::Set(clip, "name"sv, Value(a_name)) && Flash::Set(clip, "mouseEnabled"sv, Value(false)) &&
			                  Flash::Set(clip, "mouseChildren"sv, Value(false));
			return made ? clip : Value();
		}

		bool Add(Value& a_parent, Value& a_child)
		{
			return a_child.IsDisplayObject() && Flash::Call(a_parent, "addChild", std::array{ a_child });
		}

		// A filled field in a_parent and its box, or nothing.
		struct Field
		{
			Value field;
			Box   own;
		};

		[[nodiscard]] std::optional<Field> FieldIn(Scaleform::GFx::Movie& a_movie, Roles::TraderSide::Side& a_side, Value& a_parent, const char* a_name,
			const std::string& a_html)
		{
			Field made{ FieldOf(a_movie, a_side, a_name), {} };
			if (!Add(a_parent, made.field) || !Flash::Set(made.field, "htmlText"sv, Value(a_html.c_str()))) {
				return std::nullopt;
			}
			const auto own = Roles::BoundsIn(made.field, made.field);
			if (!own) {
				return std::nullopt;
			}
			made.own = *own;
			return made;
		}

		// A kind's row: the word and the percent.
		struct Row
		{
			Value clip;
			Field label;
			Field value;

			[[nodiscard]] double Tall() const { return std::max(label.own.bottom - label.own.top, value.own.bottom - value.own.top); }
		};

		void Put(Value& a_clip, double a_x, double a_y)
		{
			Flash::Set(a_clip, "x"sv, Value(a_x));
			Flash::Set(a_clip, "y"sv, Value(a_y));
		}
	}

	// -------------------------------------------------------------------
	// Building the panel
	// -------------------------------------------------------------------

	std::optional<Panel> Build(Scaleform::GFx::Movie& a_movie, Roles::TraderSide::Side& a_side, const Ceilings& a_ceilings, std::string_view& a_missing)
	{
		const auto title = TitleSize(a_side);
		if (!title) {
			a_missing = "trader's title"sv;
			return std::nullopt;
		}
		const auto size = std::lround(std::max(LEAST, SHARE * *title));
		const auto em = static_cast<double>(size);

		// A row's 2 fields are named as an item card row names its own.
		const auto names = Roles::Card::RowFieldNames();

		// Each part goes in as it is made, so the trace names the first step
		// the movie refuses. The back clip is first, under the rest.
		Panel panel{ Clip(a_movie, SIGN_NAME), Clip(a_movie, BACK_NAME) };
		auto  fill = ShapeOf(a_movie, FILL_NAME);
		auto  lines = ShapeOf(a_movie, LINES_NAME);
		if (!panel.clip.IsDisplayObject() || !Add(panel.clip, panel.back)) {
			a_missing = "Sprite class"sv;
			return std::nullopt;
		}
		// The fill only gives the back clip its box, as the list's own
		// bracket clip holds a hidden outline, so the engine's background is
		// all that shows there.
		if (!Add(panel.back, fill) || !Flash::Set(fill, "visible"sv, Value(false)) || !Add(panel.clip, lines)) {
			a_missing = "Shape class"sv;
			return std::nullopt;
		}

		auto       head = Clip(a_movie, TITLE_NAME);
		const auto any = std::ranges::any_of(KINDS, [&](const Kind& a_kind) { return a_ceilings.Of(a_kind.trade) > 0; });
		auto       heading = Add(panel.clip, head) ? FieldIn(a_movie, a_side, head, names.label, Html(Capitals(any ? Text::SignTitle() : Text::SignNone()), NUMBER_FONT, size)) :
		                                             std::nullopt;
		const auto first = heading ? Flash::FirstLine(heading->field) : std::nullopt;
		if (!heading || !first) {
			a_missing = "text field the sign can fill"sv;
			return std::nullopt;
		}

		std::vector<Row> rows;
		for (const auto& kind : KINDS) {
			const auto level = a_ceilings.Of(kind.trade);
			if (level == 0) {
				continue;
			}
			Row  row{ Clip(a_movie, kind.name), {}, {} };
			auto label = Add(panel.clip, row.clip) ? FieldIn(a_movie, a_side, row.clip, names.label, Html(Text::SignKind(kind.trade), WORD_FONT, size)) : std::nullopt;
			auto value = label ? FieldIn(a_movie, a_side, row.clip, names.value, Html(std::format("{:d}%", level), NUMBER_FONT, size)) : std::nullopt;
			if (!label || !value) {
				a_missing = "text field the sign can fill"sv;
				return std::nullopt;
			}
			row.label = std::move(*label);
			row.value = std::move(*value);
			rows.push_back(std::move(row));
		}

		// Across: the title's part of the top line or the widest row,
		// whichever is wider. A field's letters sit inside its margin.
		const auto margin = Flash::FIELD_MARGIN;
		const auto letters = [&](const Field& a_field) { return a_field.own.right - a_field.own.left - 2.0 * margin; };
		auto       wide = TITLE_IN * em + (first->right - first->left) + (TITLE_GAP + RIGHT_STUB) * em;
		for (const auto& row : rows) {
			wide = std::max(wide, 2.0 * ROW_IN * em + letters(row.label) + COLUMN_GAP * em + letters(row.value));
		}

		// Down: the title from 0 with the top line on its baseline, the rows
		// box on box, or 1 empty row's height for no repairs.
		Put(heading->field, TITLE_IN * em - first->left, 0.0);
		const auto top = first->baseline;
		auto       down = top + ROW_PAD * em;
		for (auto& row : rows) {
			Put(row.clip, 0.0, down);
			Put(row.label.field, ROW_IN * em - margin - row.label.own.left, -row.label.own.top);
			Put(row.value.field, wide - ROW_IN * em + margin - row.value.own.right, -row.value.own.top);
			down += row.Tall();
		}
		if (rows.empty()) {
			down += heading->own.bottom - heading->own.top;
		}
		const Box frame{ 0.0, top, wide, down + BOTTOM_PAD * em };

		// The title breaks the top line, as the list's title breaks the
		// list's.
		const std::pair gap{ TITLE_IN * em - TITLE_GAP * em, TITLE_IN * em + (first->right - first->left) + TITLE_GAP * em };
		Value           graphics;
		const auto      filled = fill.GetMember("graphics"sv, &graphics) && graphics.IsObject() &&
		                    graphics.Invoke("beginFill", std::array{ Value(0xFFFFFF), Value(1.0) }) &&
		                    graphics.Invoke("drawRect", std::array{ Value(frame.left), Value(frame.top), Value(frame.right - frame.left), Value(frame.bottom - frame.top) }) &&
		                    graphics.Invoke("endFill");
		if (!filled || !DrawFrame(lines, frame, gap)) {
			a_missing = "Shape that draws"sv;
			return std::nullopt;
		}
		return panel;
	}

	// -------------------------------------------------------------------
	// Placing the panel
	// -------------------------------------------------------------------

	std::optional<Spot> UnderOf(Value& a_panel, const Roles::TraderSide::Room& a_room)
	{
		const auto own = Roles::BoundsIn(a_panel, a_panel);
		const auto scale = Flash::Number(a_panel, "scaleX"sv);
		if (!own || !std::isfinite(scale) || !(scale > 0.0)) {
			return std::nullopt;
		}
		const auto top = a_room.bracket.bottom + GAP * a_room.unit;
		const auto right = RightEnd(a_room);
		Spot       spot;
		spot.x = right - own->right * scale;
		spot.y = top - own->top * scale;
		spot.box = Box{ spot.x + own->left * scale, top, right, top + (own->bottom - own->top) * scale };
		return spot;
	}
}
