#include "UI/Inventory/PaperDoll/Room.h"

#include "UI/Flash.h"
#include "UI/Roles/Roles.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace PaperDoll
{
	namespace
	{
		// How far above the icon's top the bar's middle sits: a gap of 2, plus
		// half the bar's depth, so the bar grows upward from 2 above the icon.
		[[nodiscard]] constexpr double BarRise(double a_deep)
		{
			return 2.0 + (a_deep / 2.0);
		}

		// The narrowest bar worth drawing.
		constexpr double BAR_LEAST = 20.0;

		// How wide an entry is when its parts cannot be read, the icon and the
		// number as Pipboy_InvPage.swf ships them.
		constexpr double ENTRY_WIDTH = 62.0;

		// How far under what is above it a bar's top stays.
		constexpr double HOLD = 2.0;

		// The thinnest a bar is held to, as thin as the lowest Pip-Boy CND bar
		// thickness draws it, 4.05.
		constexpr double LEAST_DEEP = BAR_DEEP * HudParts::Readout::LEAST_SIZE;

		// How many levels below the header a text may sit. Vanilla's tab
		// names are 4 down, in TabHeader_mc.AlphaHolder.Selected.
		constexpr std::uint32_t HEADER_DEPTH = 4;

		// The figure's own drawings are named, each name ending in _mc. The
		// resistance entries, ending in _Resist_mc, and NEC's own boxes,
		// starting with NEC_, do not count. Nor does the base figure, which
		// has no name of its own: its box reaches down past the torso's
		// number and would leave the torso bar no room.
		constexpr std::string_view CLIP_END = "_mc"sv;
		constexpr std::string_view ENTRY_END = "_Resist_mc"sv;
		constexpr std::string_view OWN_START = "NEC_"sv;

		[[nodiscard]] bool IsFigureClip(std::string_view a_name)
		{
			return a_name.ends_with(CLIP_END) && !a_name.ends_with(ENTRY_END) && !a_name.starts_with(OWN_START);
		}

		// A box worth reading: finite and with some size. A hidden figure
		// can give NaN or 0 high.
		[[nodiscard]] bool GivesBox(const Edges& a_box)
		{
			return a_box.found && std::isfinite(a_box.left) && std::isfinite(a_box.right) && std::isfinite(a_box.top) &&
			       std::isfinite(a_box.bottom) && a_box.bottom > a_box.top;
		}

		// Whether a box was read and overlaps a bar from a_left to a_right
		// across.
		[[nodiscard]] bool Across(const Edges& a_box, double a_left, double a_right)
		{
			return a_box.found && a_box.left < a_right && a_left < a_box.right;
		}

		// A text field, by the text it holds, and no clip that only keeps a
		// text member of its own.
		[[nodiscard]] bool IsText(const Value& a_clip)
		{
			return Flash::Member(a_clip, "text"sv).IsString() && !Flash::IsAnyNumber(Flash::Member(a_clip, "numChildren"sv));
		}

		[[nodiscard]] bool HasLetters(std::string_view a_text)
		{
			return a_text.find_first_not_of(" \t\r\n"sv) != std::string_view::npos;
		}

		// Whether a header text's first line ends at its baseline: only the
		// capitals A to Z, digits and spaces, as vanilla's English tab names
		// are, none of which hangs below it. Any other line, with lowercase,
		// accented letters, Cyrillic, Japanese or Chinese, can draw below the
		// baseline, so it ends at its baseline plus its descent.
		[[nodiscard]] bool EndsAtBaseline(std::string_view a_text)
		{
			const auto line = a_text.substr(0, a_text.find_first_of("\r\n"sv));
			return std::ranges::all_of(line, [](char a_c) { return a_c == ' ' || (a_c >= 'A' && a_c <= 'Z') || (a_c >= '0' && a_c <= '9'); });
		}
	}

	Edges EdgesIn(Value& a_clip, Value& a_space)
	{
		Value rect;
		if (!a_clip.IsDisplayObject() || !a_clip.Invoke("getBounds", &rect, &a_space, 1) || !rect.IsObject()) {
			return {};
		}
		Edges out;
		out.left = Flash::Number(rect, "x"sv);
		out.top = Flash::Number(rect, "y"sv);
		out.right = out.left + Flash::Number(rect, "width"sv);
		out.bottom = out.top + Flash::Number(rect, "height"sv);
		out.found = out.right > out.left;
		return out;
	}

	// -------------------------------------------------------------------
	// Where a bar sits over its entry
	// -------------------------------------------------------------------

	Placement PlaceOver(Value& a_entry, Value& a_icon, Value& a_value, double a_digits, double a_deep)
	{
		const auto shield = EdgesIn(a_icon, a_entry);
		const auto box = EdgesIn(a_value, a_entry);
		if (!shield.found && !box.found) {
			return Placement{ ENTRY_WIDTH, -BarRise(a_deep), ENTRY_WIDTH, a_deep, false };
		}

		const auto left = shield.found ? shield.left : box.left;
		const auto top = shield.found ? shield.top : box.top;
		const auto right = box.found ? box.left + Flash::FIELD_MARGIN + a_digits : shield.right;

		Placement out;
		out.x = std::max(right, left + BAR_LEAST);
		out.y = top - BarRise(a_deep);
		out.width = out.x - left;
		out.deep = a_deep;
		out.measured = true;
		return out;
	}

	// -------------------------------------------------------------------
	// The header's texts
	// -------------------------------------------------------------------

	namespace
	{
		// A hidden clip draws nothing, so it is skipped with all below it.
		void AddTexts(Value& a_clip, std::uint32_t a_depth, std::vector<Value>& a_texts)
		{
			for (auto& child : Flash::Children(a_clip)) {
				if (!Flash::Bool(child, "visible"sv)) {
					continue;
				}
				if (IsText(child)) {
					if (HasLetters(Flash::String(child, "text"sv))) {
						a_texts.push_back(child);
					}
				} else if (a_depth < HEADER_DEPTH) {
					AddTexts(child, a_depth + 1, a_texts);
				}
			}
		}
	}

	std::vector<Value> HeaderTexts(Value& a_header)
	{
		std::vector<Value> texts;
		if (a_header.IsDisplayObject() && Flash::Bool(a_header, "visible"sv)) {
			AddTexts(a_header, 1, texts);
		}
		return texts;
	}

	// The field's box only says whether it overlaps the bar across. How low
	// it reaches is where its first line's drawn letters end, see
	// EndsAtBaseline, mapped through the stage into the entry.
	std::optional<Limit> HeaderAbove(Scaleform::GFx::Movie& a_movie, std::vector<Value>& a_texts, Value& a_entry, double a_bottom, double a_left, double a_right)
	{
		std::optional<Limit> lowest;
		for (auto& field : a_texts) {
			if (!Across(EdgesIn(field, a_entry), a_left, a_right)) {
				continue;
			}
			const auto text = Flash::String(field, "text"sv);
			const auto line = Flash::DrawnLine(field);
			const auto end = line ? line->baseline + (EndsAtBaseline(text) ? 0.0 : line->descent) : 0.0;
			const auto onStage = line ? Roles::ToStage(a_movie, field, Flash::Point{ (line->left + line->right) / 2.0, end }) : std::nullopt;
			const auto inEntry = onStage ? Roles::FromStage(a_movie, a_entry, *onStage) : std::nullopt;
			if (inEntry && inEntry->y < a_bottom && (!lowest || inEntry->y > lowest->at)) {
				lowest = Limit{ inEntry->y, text, true };
			}
		}
		return lowest;
	}

	// -------------------------------------------------------------------
	// The room above a bar
	// -------------------------------------------------------------------

	// A hidden clip still gives its box, so whether a limb is worn or picked
	// never changes the room, and a layout never goes stale on the hovered
	// item.
	Room RoomAbove(Scaleform::GFx::Movie& a_movie, Value& a_doll, std::vector<Value>& a_texts, Value& a_entry, double a_bottom, double a_left, double a_right)
	{
		Room room;
		room.header = HeaderAbove(a_movie, a_texts, a_entry, a_bottom, a_left, a_right);
		room.noClips = true;
		for (auto& child : Flash::Children(a_doll)) {
			const auto name = Flash::String(child, "name"sv);
			if (!IsFigureClip(name)) {
				continue;
			}
			room.noClips = false;
			const auto box = EdgesIn(child, a_entry);
			if (!GivesBox(box)) {
				if (room.noBox.empty()) {
					room.noBox = name;
				}
				continue;
			}
			if (Across(box, a_left, a_right) && box.bottom < a_bottom && (!room.clip || box.bottom > room.clip->at)) {
				room.clip = Limit{ box.bottom, name, false };
			}
		}
		return room;
	}

	Hold HoldIn(const Room& a_room, double a_bottom, double a_asked)
	{
		Hold hold{ a_room.noClips || !a_room.noBox.empty() ? std::min(a_asked, NO_CLIPS_DEEP) : a_asked, std::nullopt, std::nullopt };

		// The lower of the 2 limits is the nearer one, y runs down.
		const auto& lower = !a_room.header || (a_room.clip && a_room.clip->at >= a_room.header->at) ? a_room.clip : a_room.header;
		if (lower) {
			const auto room = a_bottom - lower->at - HOLD;
			if (room < hold.deep) {
				hold.deep = std::max(room, LEAST_DEEP);
				hold.held = lower;
				if (room < LEAST_DEEP) {
					hold.tight = room;
				}
			}
		}
		return hold;
	}
}
