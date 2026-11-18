#pragma once

#include "Core/Plugin.h"

#include "UI/Hud/HudParts/Readout.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Where each CND bar on the Pip-Boy apparel figure sits over its entry, and how
// far it may grow upward before it meets the figure or a tab name, read when a
// bar is laid out, in the units of the entry it hangs on, y down. And each
// bar's state and its layout there.
//
// A bar stops 2 under 2 kinds of thing. The figure's named clips, the body
// part and head gear drawings, count shown or hidden: vanilla hides a limb no
// piece covers, while the unnamed base figure still draws the feet and hands
// there. The header's texts count by their drawn letters, never by their box,
// since a tab name's box can reach well below its letters. Private to this
// folder.
namespace PaperDoll
{
	using Value = Scaleform::GFx::Value;

	// A clip's edges in another clip's coordinates, through getBounds, so
	// where the icon's own drawing puts its origin does not matter. A clip
	// with nothing drawn has no edges.
	struct Edges
	{
		double left{ 0.0 };
		double top{ 0.0 };
		double right{ 0.0 };
		double bottom{ 0.0 };
		bool   found{ false };
	};

	[[nodiscard]] Edges EdgesIn(Value& a_clip, Value& a_space);

	// Where a bar goes over an entry: from the icon's left edge to where the
	// digits end, just above the icon. The number's box is sized for the
	// longest number and ends well past the digits, which start at its left
	// margin. A readout's origin is the right end of its bar, see
	// HudParts::Readout. measured is false when the entry's parts cannot be
	// read, and the bar then goes where vanilla ships them.
	struct Placement
	{
		double x{ 0.0 };
		double y{ 0.0 };
		double width{ 0.0 };
		double deep{ 0.0 };
		bool   measured{ false };

		bool operator==(const Placement&) const = default;
	};

	// Where a bar a_deep deep goes over a_entry, whose icon is a_icon and
	// whose number a_value shows digits a_digits wide. The bar's bottom sits
	// 2 above the icon at every depth, so a deeper bar grows upward.
	[[nodiscard]] Placement PlaceOver(Value& a_entry, Value& a_icon, Value& a_value, double a_digits, double a_deep);

	// How wide the number's digits are. It changes with the resistance and
	// with the damage type the player cycles to.
	[[nodiscard]] double DigitsWidth(Value& a_entry);

	// How deep a bar is at Apparel figure CND bar thickness 1: sized for the
	// number next to the icon, 18 high, as every CND bar is sized for its text,
	// see HudParts::Readout, and then half again, 8.1. Its word is never
	// written, so the bar shows alone.
	inline constexpr double BAR_DEEP = HudParts::Readout::BAR_DEEP * 18.0 * 1.5;

	// How deep a bar grows at most while the figure's clips cannot be read,
	// measured to clear the figure's feet in every layout seen.
	inline constexpr double NO_CLIPS_DEEP = 10.8;

	// Something above a bar: the y where it ends, and its name, a clip's
	// name or a header text, whose letters end there.
	struct Limit
	{
		double      at{ 0.0 };
		std::string under;
		bool        letters{ false };

		bool operator==(const Limit&) const = default;
	};

	// What is above 1 bar: the lowest named clip and the lowest header text,
	// each only when it ends above the bar's bottom and overlaps it across.
	// The bar grows to NO_CLIPS_DEEP at most while the figure has no named
	// clip at all, noClips, or a named clip gives no box, noBox, as a hidden
	// figure can.
	struct Room
	{
		std::optional<Limit> clip;
		std::optional<Limit> header;
		bool                 noClips{ false };
		std::string          noBox;
	};

	// The visible texts with letters in the Pip-Boy's header, a_header, up to
	// 4 levels down. Read once a frame and shared by the 6 bars. None when
	// a_header is no clip.
	[[nodiscard]] std::vector<Value> HeaderTexts(Value& a_header);

	// The room above a bar on a_entry from a_left to a_right across whose
	// bottom is at a_bottom. a_doll is the figure, a_texts what HeaderTexts
	// gave.
	[[nodiscard]] Room RoomAbove(Scaleform::GFx::Movie& a_movie, Value& a_doll, std::vector<Value>& a_texts, Value& a_entry, double a_bottom, double a_left, double a_right);

	// The header part of RoomAbove alone, so a header built or moved after
	// the layout can be told from the one the bar was laid out under.
	[[nodiscard]] std::optional<Limit> HeaderAbove(Scaleform::GFx::Movie& a_movie, std::vector<Value>& a_texts, Value& a_entry, double a_bottom, double a_left, double a_right);

	// How deep a bar is drawn, and the limit that held it thinner than asked,
	// if one did. tight is the room left under that limit when it is less
	// than the bar's least depth, so the bar's top covers the limit.
	struct Hold
	{
		double                deep{ 0.0 };
		std::optional<Limit>  held;
		std::optional<double> tight;
	};

	// a_asked held so the bar's top stays 2 under the lower limit of a_room,
	// for a bar whose bottom is at a_bottom. Never under the depth Pip-Boy CND
	// bar thickness 0.5 draws, half of BAR_DEEP, even where the room is less,
	// and at most NO_CLIPS_DEEP while the clips cannot be read.
	[[nodiscard]] Hold HoldIn(const Room& a_room, double a_bottom, double a_asked);

	// A value no region ever holds, so the first frame always draws.
	inline constexpr std::int32_t NOT_DRAWN = -2;

	// One bar's state, kept between frames so only a change is written.
	// The digits and the thickness are what the bar was laid out for, -1
	// before that, the room what it found above it then and held what
	// held it thinner. tight is the room under held when the bar covers
	// it, see Hold, and tightSaid what the trace last named of it.
	// shownMark and shownStill are the mark and the still end last
	// drawn, forgotten with shown.
	struct Bar
	{
		Placement                placed;
		double                   digits{ -1.0 };
		double                   thickness{ -1.0 };
		Room                     room;
		std::optional<Limit>     held;
		std::optional<double>    tight;
		std::optional<double>    tightSaid;
		std::int32_t             shown{ NOT_DRAWN };
		HudParts::Readout::Mark  shownMark;
		HudParts::Readout::Still shownStill{ HudParts::Readout::Still::kLeftEnd };
	};

	// Whether the room above a laid out bar may have changed: a named
	// clip gave no box at its layout, or the header's letters over it
	// came, went or moved, as a header built after the figure does.
	// Asked every few frames, see PaperDoll.cpp.
	[[nodiscard]] bool Stale(Scaleform::GFx::Movie& a_movie, std::vector<Value>& a_texts, Value& a_entry, const Bar& a_bar);

	// Places a bar over its entry for the digits the number shows now,
	// as deep as a_thickness asks and the room above it lets it be. Its
	// bottom stays put and it grows upward. False when the layout comes
	// out as the last one, which then changes nothing.
	bool Lay(Scaleform::GFx::Movie& a_movie, Value& a_readout, Value& a_doll, std::vector<Value>& a_texts, Value& a_entry, Bar& a_bar, double a_digits, double a_thickness);

	// How high something whose bottom is at a_bottom may stand under the
	// lower limit of a_room, kept 2 under it as HoldIn keeps a bar, and the
	// limit, at most NO_CLIPS_DEEP with no limit while the clips cannot be
	// read. Nothing when nothing above holds it.
	struct Headroom
	{
		double               high{ 0.0 };
		std::optional<Limit> under;
	};
	[[nodiscard]] std::optional<Headroom> HeadroomOf(const Room& a_room, double a_bottom);

	// How long the bar over a_entry is while its number shows 1 digit, for
	// a bar a_deep deep. Digits share 1 width, so 1 is the number's width
	// over its count of characters.
	[[nodiscard]] double ShortWidth(Value& a_entry, double a_deep);
}
