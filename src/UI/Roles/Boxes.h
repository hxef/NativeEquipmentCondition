#pragma once

#include "UI/Roles/Roles.h"

#include <optional>
#include <string>

// The bench's confirmation box, as NEC grows it, and the background of the
// game's message box.
namespace Roles::Boxes
{
	// The parts of the box, the engine's confirmObj, that a growth moves.
	struct Parts
	{
		Value  background;
		Value  buttons;
		Value  arrow;
		double listEnd = 0.0;  // the last row's originalY plus its height
	};

	// Nothing while every row is shown, since the box needs no growth then.
	// The panel adds each row as its newest child, so the last child is the
	// last row. Nothing until that child has an originalY and every number
	// read is finite, or while the background is not on screen, so a box that
	// lays out late or draws a background of its own is left as it is.
	[[nodiscard]] std::optional<Parts> ConfirmPanel(RE::ExamineConfirmMenu& a_menu);

	// What the box asks, on one line, for the trace.
	[[nodiscard]] std::string Question(RE::ExamineConfirmMenu& a_menu);

	// Where the open bench's button bar starts, as a share of the screen's
	// height from its top, whichever movie draws the bar. Nothing while no bar
	// is on screen, and 1 trace line says why.
	[[nodiscard]] std::optional<double> BenchBarTop();

	// The message box's background clip, at vanilla's place, or a value that
	// is not a display object.
	[[nodiscard]] Value MessageBackground(RE::MessageBoxMenu& a_menu);
}
