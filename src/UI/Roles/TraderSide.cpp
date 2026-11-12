#include "UI/Roles/TraderSide.h"

#include "UI/Flash.h"

#include <algorithm>
#include <string_view>

namespace Roles::TraderSide
{
	namespace
	{
		// Vanilla's place, read off BarterMenu.swf, and the bracket inside it.
		constexpr const char* SIDE_PATH = "_root.FilterHolder_mc.Menu_mc.ContainerInventory_mc";
		constexpr const char* BRACKET_NAME = "ContainerBracketBackground_mc";

		// The title over the trader's list: a clip with the text field and
		// the 2 arrows that switch the list's tab.
		constexpr std::string_view HEADER_NAME = "ContainerListHeader"sv;
		constexpr std::string_view TITLE_NAME = "textField"sv;

		// Every part NEC adds is named NEC_..., and none counts as taken.
		constexpr std::string_view OWN_PREFIX = "NEC_"sv;

		// The share of the bracket's area a part must cover to count as its
		// background or its frame lines. Not all of it, since the bracket's
		// bounds count its own outline and a filled copy of it can sit off
		// those bounds by that outline. A share holds at any scale.
		constexpr double COVER = 0.98;

		[[nodiscard]] bool IsText(const Value& a_clip)
		{
			return Flash::Member(a_clip, "text"sv).IsString() && !Flash::IsAnyNumber(Flash::Member(a_clip, "numChildren"sv));
		}

		// Whether a_part covers at least COVER of a_bracket's area.
		[[nodiscard]] bool Covers(const Box& a_part, const Box& a_bracket)
		{
			const auto across = std::min(a_part.right, a_bracket.right) - std::max(a_part.left, a_bracket.left);
			const auto down = std::min(a_part.bottom, a_bracket.bottom) - std::max(a_part.top, a_bracket.top);
			const auto area = (a_bracket.right - a_bracket.left) * (a_bracket.bottom - a_bracket.top);
			return across > 0.0 && down > 0.0 && across * down >= COVER * area;
		}

		// What a_part draws in a_side's units, as Room::taken says. A text
		// field counts from its first letter to its last across, since a
		// title leaves most of its box empty, and down its whole box. With 2
		// lines or more it counts whole. The HUD's Drawn counts a field
		// across its whole width instead, so a bar beside it stays put as a
		// number changes.
		void Add(std::vector<Box>& a_taken, Value& a_part, Value& a_side)
		{
			const auto box = BoundsIn(a_part, a_side);
			if (!box) {
				return;
			}
			if (!IsText(a_part)) {
				a_taken.push_back(*box);
				return;
			}
			if (Flash::String(a_part, "text"sv).empty()) {
				return;
			}
			const auto own = BoundsIn(a_part, a_part);
			const auto line = own && Flash::Number(a_part, "numLines"sv) <= 1.0 ? Flash::FirstLine(a_part) : std::nullopt;
			if (!line) {
				a_taken.push_back(*box);
				return;
			}
			const auto across = (box->right - box->left) / (own->right - own->left);
			a_taken.push_back(Box{ box->left + (line->left * across), box->top, box->left + (line->right * across), box->bottom });
		}

		// Fills a_room's taken from a_side's parts, as Room says, and sets its
		// top, the frame's top: the highest of the bracket's own top while the
		// bracket shows and the tops of the parts that cover it, its frame
		// lines among them. A covering part counts only while it starts no
		// higher than the title, since one that starts higher is a backdrop
		// behind the title's row. With nothing counted, top is the bracket's.
		// A covering part that starts between the title's top and the
		// bracket's still lifts the line. When the bracket is hidden and the
		// only covering part starts lower than the bracket's top, top follows
		// that part, so the line sits lower by as much.
		void Take(Room& a_room, Side& a_side)
		{
			auto&                 side = a_side.side;
			const auto            title = BoundsIn(a_side.title, side);
			std::optional<double> top;
			const auto            lift = [&](double a_top) { top = std::min(top.value_or(a_top), a_top); };
			if (Flash::Opacity(a_side.bracket) > 0.0) {
				lift(a_room.bracket.top);
			}
			for (auto& child : Flash::Children(side)) {
				if (!(Flash::Opacity(child) > 0.0) || Flash::String(child, "name"sv).starts_with(OWN_PREFIX)) {
					continue;
				}
				const auto box = BoundsIn(child, side);
				if (!box) {
					continue;
				}
				if (Covers(*box, a_room.bracket)) {
					if (title && box->top >= title->top) {
						lift(box->top);
					}
					continue;
				}
				// A clip's parts count shown or hidden, since the title shows
				// its arrows only while its list has the focus, and a sign that
				// came and went with the focus would jump. A clip whose parts
				// all hide draws nothing, as a row of tab slots with no icons
				// to show.
				if (IsText(child) || !(Flash::Number(child, "numChildren"sv) > 0.0)) {
					Add(a_room.taken, child, side);
					continue;
				}
				auto parts = Flash::Children(child);
				if (std::ranges::none_of(parts, [](const Value& a_part) { return Flash::Opacity(a_part) > 0.0; })) {
					continue;
				}
				for (auto& part : parts) {
					Add(a_room.taken, part, side);
				}
			}
			a_room.top = top.value_or(a_room.bracket.top);
		}
	}

	std::optional<Side> Find(RE::ContainerMenuBase& a_menu, bool a_quiet)
	{
		if (!a_menu.uiMovie) {
			return std::nullopt;
		}
		auto& movie = *a_menu.uiMovie;

		// The title is read off whichever side is found, and kept only when
		// it is a text field.
		const auto with = [](Value a_side, Value a_bracket) {
			auto title = Flash::Member(Flash::Member(a_side, HEADER_NAME), TITLE_NAME);
			return Side{ a_side, a_bracket, IsText(title) ? title : Value() };
		};

		const auto* side = a_menu.containerInventory_mc.get();
		const auto* bracket = a_menu.containerBracketBackground_mc.get();
		if (side && bracket && side->IsDisplayObject() && bracket->IsDisplayObject()) {
			if (!a_quiet) {
				Found(movie, "trader's side"sv, "through the engine"sv);
			}
			return with(*side, *bracket);
		}

		Value placed;
		if (!movie.GetVariable(&placed, SIDE_PATH) || !placed.IsDisplayObject()) {
			return std::nullopt;
		}
		auto held = Flash::Member(placed, BRACKET_NAME);
		if (!held.IsDisplayObject()) {
			return std::nullopt;
		}
		if (!a_quiet) {
			Found(movie, "trader's side"sv, "at vanilla's place"sv);
		}
		return with(placed, held);
	}

	bool Room::Fits(const Box& a_box, double a_gap, double a_under, bool a_around) const
	{
		const auto gap = a_gap * unit;
		const Box  grown{ a_box.left - gap, a_box.top - gap, a_box.right + gap, a_box.bottom + (a_under * unit) };
		const auto clear = [&](const std::vector<Box>& a_parts) { return std::ranges::none_of(a_parts, [&](const Box& a_part) { return grown.Meets(a_part); }); };
		return frame && frame->Holds(a_box) && clear(taken) && (!a_around || clear(around));
	}

	std::optional<Room> RoomOf(Scaleform::GFx::Movie& a_movie, Side& a_side, Value a_bar)
	{
		auto&      side = a_side.side;
		const auto bracket = BoundsIn(a_side.bracket, side);
		const auto origin = ToStage(a_movie, side, Point{ 0.0, 0.0 });
		const auto down = ToStage(a_movie, side, Point{ 0.0, 1.0 });
		if (!bracket || !origin || !down || !(down->y - origin->y > 0.0)) {
			return std::nullopt;
		}
		Room room{ FrameIn(a_movie, side), *bracket, bracket->top, {}, 1.0 / (down->y - origin->y), {} };
		Take(room, a_side);
		if (const auto bar = BoundsIn(a_bar, side)) {
			room.taken.push_back(*bar);
		}
		return room;
	}

	void AroundOf(Side& a_side, Room& a_room)
	{
		a_room.around.clear();
		auto& side = a_side.side;
		auto  parent = Flash::Member(side, "parent"sv);
		if (!parent.IsDisplayObject()) {
			return;
		}
		const auto drawn = [](const Value& a_part) {
			return Flash::Opacity(a_part) > 0.0 && !Flash::String(a_part, "name"sv).starts_with(OWN_PREFIX);
		};
		for (auto& child : Flash::Children(parent)) {
			// The side holds itself, and taken counts it already.
			Value holds;
			if (!drawn(child) || (child.Invoke("contains", &holds, &side, 1) && holds.IsBoolean() && holds.GetBoolean())) {
				continue;
			}
			if (IsText(child) || !(Flash::Number(child, "numChildren"sv) > 0.0)) {
				Add(a_room.around, child, side);
				continue;
			}
			for (auto& part : Flash::Children(child)) {
				if (drawn(part)) {
					Add(a_room.around, part, side);
				}
			}
		}
	}
}
