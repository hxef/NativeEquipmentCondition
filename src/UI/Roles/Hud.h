#pragma once

#include "UI/Roles/Roles.h"

#include <optional>
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

	// The divider between the counter's 2 numbers, measured now. a_ownVisible
	// is the movie's own visible while NEC's bar covers the divider, so NEC's
	// hide is not taken for the movie's. Nothing while the movie hides it, it
	// is off screen or not laid out, or its own rotation is not 0, since a
	// turned divider measures about its thickness across.
	[[nodiscard]] std::optional<Line> Divider(Scaleform::GFx::Movie& a_movie, Value& a_counter, std::optional<bool> a_ownVisible);

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
