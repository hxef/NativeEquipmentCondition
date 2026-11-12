#pragma once

#include "UI/Roles/Roles.h"

#include <optional>
#include <vector>

// The trader's side of a container or barter screen: the clip that holds the
// trader's caps, the bracket around their list and the bracket's lines, and
// that bracket's background. The engine colours the side and every child of
// it in the HUD colour.
namespace Roles::TraderSide
{
	struct Side
	{
		Value side;     // holds the caps, the bracket and its lines
		Value bracket;  // the bracket's background
		Value title;    // the text field over the list, not a display object when the movie has none
	};

	// The engine's 2 clips, else the side at vanilla's place when it holds a
	// bracket. Nothing for a movie with neither. a_quiet leaves out the trace
	// line that says how the side was found, for a caller asking each frame.
	[[nodiscard]] std::optional<Side> Find(RE::ContainerMenuBase& a_menu, bool a_quiet = false);

	// What a_side's screen leaves free, in the side's units.
	struct Room
	{
		std::optional<Box> frame;  // what the screen shows, nothing when the side is turned
		Box                bracket;
		double             top = 0.0;  // the frame's top: the highest top of the shown bracket and of the covering parts that start no higher than the title

		// What the side draws, 1 box per part: a text field by its letters,
		// a clip by its parts 1 level down, anything else by its bounds, and
		// the bar's bounds. A child of the side hidden or faded to 0 counts
		// as nothing, a clip whose parts are all hidden and an empty text
		// field neither. Parts that cover the
		// bracket, its background and frame lines, and NEC's own are left
		// out, so a box that meets none of these is free.
		std::vector<Box> taken;
		double           unit = 0.0;  // the side's units per stage px

		// What the rest of the screen draws, filled by AroundOf: each other
		// child of the side's parent, the other side and the item card among
		// them, by its shown parts 1 level down, where taken counts hidden
		// ones too, since a movie can keep buttons hidden there that it
		// shows on some tabs only, so such a button counts only while it
		// shows.
		std::vector<Box> around;

		// Whether the screen shows all of a_box and a gap of a_gap stage
		// px left, right and above it and of a_under under it meets nothing
		// taken, and with a_around nothing around either.
		[[nodiscard]] bool Fits(const Box& a_box, double a_gap, double a_under, bool a_around = false) const;
	};

	// a_bar counts as taken when its bounds read. Nothing when the
	// bracket's box or the side's scale does not read as numbers.
	[[nodiscard]] std::optional<Room> RoomOf(Scaleform::GFx::Movie& a_movie, Side& a_side, Value a_bar = {});

	// Fills a_room's around, a few reads per part, so it is asked only for
	// a place outside the side.
	void AroundOf(Side& a_side, Room& a_room);
}
