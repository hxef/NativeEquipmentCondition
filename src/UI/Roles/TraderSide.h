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

		// Whether the bracket shows, so the engine paints the list's see
		// through background over its box. A movie that paints its own
		// hides it.
		bool backed = false;

		// The list's frame lines while they show and cover the bracket:
		// the side's child the movie's script draws them in, by vanilla's
		// name, which the UI replacers seen so far keep. Nothing in a movie
		// that draws none.
		std::optional<Box> lines;

		// The bar's bounds, nothing when they do not read.
		std::optional<Box> bar;

		// The top of the row the button bar is drawn in, nothing when it
		// does not read. Vanilla's bar is drawn by another menu, see
		// Bars::DrawnTop, while the movie's own bar stays hidden lower down.
		std::optional<double> row;

		// What the side draws, 1 box per part: a text field by its letters,
		// a clip by its parts 1 level down, anything else by its bounds, and
		// the bar's bounds. A child of the side hidden or faded to 0 counts
		// as nothing, a clip whose parts are all hidden and an empty text
		// field neither. Parts that cover the bracket, its background and
		// frame lines, and NEC's own are left out, so a box that meets none
		// of these is free.
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

		// Whether a_box and a gap of a_gap stage px under it end above the
		// bar's row, however far across the bar reaches, so a part under the
		// list stays above it. The bar's own top where the row does not read,
		// and true when neither does.
		[[nodiscard]] bool OverBar(const Box& a_box, double a_gap) const;
	};

	// a_bar counts as taken and is kept when its bounds read. a_row is the
	// share of the screen's height where the bar is drawn, from
	// Bars::DrawnTop. Nothing when the bracket's box or the side's scale does
	// not read as numbers.
	[[nodiscard]] std::optional<Room> RoomOf(Scaleform::GFx::Movie& a_movie, Side& a_side, Value a_bar = {}, std::optional<double> a_row = std::nullopt);

	// Puts a_clip in the side's parent right above the side, so it draws over
	// what the side draws over and under what draws over the side. False when
	// the side has no parent or the movie refuses a step.
	bool AddAbove(Side& a_side, Value& a_clip);

	// Fills a_room's around, a few reads per part, so it is asked only for
	// a place outside the side.
	void AroundOf(Side& a_side, Room& a_room);
}
