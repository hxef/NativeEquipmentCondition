#pragma once

#include "UI/Roles/Roles.h"

#include <optional>
#include <string>
#include <vector>

// The bench's confirmation box, as NEC grows it, the background and the button
// list of the vanilla message box, and the barter screen's quantity box.
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

	// Lets a question that wraps show its 2nd line. Vanilla's field is a few
	// units short of 2 lines, so it draws only the 1st. The rows start at the
	// field's old bottom, so it grows only into the gap above their letters
	// and never past 2 lines. The field's new height, or nothing when the
	// question already shows 2 lines, has 1 or was not found.
	[[nodiscard]] std::optional<double> ShowSecondLine(RE::ExamineConfirmMenu& a_menu);

	// Where the open bench's button bar starts, as a share of the screen's
	// height from its top, whichever movie draws the bar. Nothing while no bar
	// is on screen, and 1 trace line says why.
	[[nodiscard]] std::optional<double> BenchBarTop();

	// The message box's background clip, at vanilla's place, or a value that
	// is not a display object.
	[[nodiscard]] Value MessageBackground(RE::MessageBoxMenu& a_menu);

	// The message box's list of buttons, at vanilla's place, or a value that
	// is not a display object.
	[[nodiscard]] Value MessageList(RE::MessageBoxMenu& a_menu);

	// A row's text field as the message box draws it.
	struct RowField
	{
		double width = 0.0;
		double textWidth = 0.0;
		double lines = 0.0;
	};

	// The text field of each row in the message box's list of buttons, all
	// those at the depth where the first is found, or none.
	[[nodiscard]] std::vector<RowField> MessageRows(RE::MessageBoxMenu& a_menu);

	// Whether a_menu's quantity box is open. A movie with no quantity box at
	// vanilla's place reads as closed.
	[[nodiscard]] bool QuantityOpen(RE::IMenu& a_menu);
}
