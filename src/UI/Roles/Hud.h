#pragma once

#include "UI/Flash.h"
#include "UI/Roles/Roles.h"

#include <optional>
#include <string>
#include <vector>

// The HUD parts NEC's readouts and meters follow.
namespace Roles::Hud
{
	// The clip the engine binds its ammo counter to, when it is a display
	// object still on the HUD. A movie with no counter there has none the
	// engine drives. OnScreen is not asked, since a melee weapon or the HUD
	// mode hides the counter on purpose.
	[[nodiscard]] std::optional<Value> AmmoCounter(Scaleform::GFx::Movie& a_movie);

	// A place in the counter's own units, which the bar's placing expects,
	// with its origin on its right end.
	struct Line
	{
		double x = 0.0;
		double y = 0.0;
		double width = 0.0;

		bool operator==(const Line&) const = default;
	};

	// The divider between the counter's 2 numbers, in the counter's units.
	// place is where its origin sits and how wide its bounds are there, and
	// box its bounds. length and thick are its own size, the same however it
	// is turned, with thick rounded to a twip, the 20th of a pixel Flash
	// keeps sizes in.
	struct DividerShape
	{
		Line   place;
		Box    box;
		double length = 0.0;
		double thick = 0.0;
		double turn = 0.0;  // its rotation in degrees, 0 upright
	};

	// The divider, measured now. a_ownVisible is the movie's own visible
	// while NEC's bar covers the divider, so NEC's hide is not taken for the
	// movie's. Nothing while the movie hides it or it is off screen or not
	// laid out.
	[[nodiscard]] std::optional<DividerShape> Divider(Scaleform::GFx::Movie& a_movie, Value& a_counter, std::optional<bool> a_ownVisible);

	// A HUD part by its instance name and its box in the counter's units.
	// shown is false for a number faded or switched off now.
	struct Part
	{
		std::string name;
		Box         box;
		bool        shown = true;
	};

	// The counter's numbers, its text fields, shown or not, so a number that
	// drops out for a moment still tells where the divider runs. Each box
	// runs the width of its field inside the margins, so it stays put as the
	// count changes, and from the top of the field's first line down to the
	// lowest its baseline can sit, where digits end. A turned field, or one
	// whose line cannot be measured, counts by its whole bounds.
	[[nodiscard]] std::vector<Part> Numbers(Value& a_counter);

	// Every other part in the counter's parent but a_ours, by what it draws
	// now: a text field as the numbers are, every line of it, a clip by its
	// pieces, anything else by its bounds. A piece switched off or faded to
	// 0 draws nothing, and with a_frame, the screen in the counter's units,
	// neither does one off screen. A part that draws nothing is left out, so
	// a hidden part or a copy a HUD mod keeps far off screen moves nothing.
	[[nodiscard]] std::vector<Part> Beside(Value& a_counter, const Value& a_ours, const std::optional<Box>& a_frame);

	// The counter's other children, neither a number nor its divider, such
	// as a box a HUD mod draws around it, measured as Beside does.
	[[nodiscard]] std::vector<Part> Inside(Value& a_counter, const std::optional<Box>& a_frame);

	// The colour the divider is drawn in, as the counter's parent sees it:
	// the counter's colour over the divider's own, or the counter's alone
	// with no divider. A HUD mod may colour either. Nothing when the counter
	// has none to read.
	[[nodiscard]] std::optional<Flash::Tint> DividerTint(Value& a_counter);

	// Hides the divider while NEC's bar covers it, asked every frame and
	// writing only on a change, so a movie that shows it again is hidden the
	// next frame. a_ownVisible keeps the movie's own visible from before and
	// gives it back when the bar moves off, so a hide by visible alone made
	// while covered is undone then.
	void Cover(Value& a_counter, bool a_covered, std::optional<bool>& a_ownVisible);

	// Where the shown text of a quick container row ends: the largest
	// x + getLineMetrics(0).x + getLineMetrics(0).width over its shown text
	// fields that are not empty, the text's own width and not the field's. A
	// name drawn in 2 fields, or with its own field hidden, still has an end.
	// With no such field, an empty name ends where vanilla's name field puts
	// its empty line.
	[[nodiscard]] std::optional<double> TextEnd(Scaleform::GFx::Movie& a_movie, Value& a_row);

	// Where the shown text of a row's name starts, and the shown children of
	// the row whose right edge is within 4 of it, a_name and a_ours left out.
	// A holder of icons counts by its shown children only.
	struct Start
	{
		double             x = 0.0;
		std::vector<Value> against;
	};
	[[nodiscard]] std::optional<Start> TextStart(Scaleform::GFx::Movie& a_movie, Value& a_row, Value& a_name, const Value& a_ours);
}
