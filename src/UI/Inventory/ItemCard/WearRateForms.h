#pragma once

#include "Core/Plugin.h"

#include <optional>
#include <string>

// The 2 forms of WriteWearRate in WearRate.cpp that measure the CND row: the
// rate in the number's own field, and in the cell beside it. Private to this
// folder.
namespace ItemCard
{
	// How form A wrote its text: how much wider the field grew, under 0
	// when a shrunk text's field narrows, and the size the text shows
	// at, 1 at its natural size.
	struct Fit
	{
		double wider;
		double size;
	};

	// Form A: a_text, the percent and the rate, in the number's own field
	// at its natural size. The field grows left with its right edge kept,
	// only while the row shows nothing but its background in that stretch,
	// and its text starts no further left than the label's last letter and
	// LABEL_GAP, wherever the field's box starts. The text is taken as
	// right aligned, as vanilla's number is, which puts it furthest left.
	// With a_mayShrink, for the rate with its sign, a text with no room
	// at its natural size may be drawn smaller by the field's own shrink,
	// down to SMALLEST_SIZE, in a field from the label's last letter and
	// LABEL_GAP to its right edge.
	// How it fit, or nothing when it would not fit or would shrink too
	// far, with the field as it was.
	[[nodiscard]] std::optional<Fit> WriteInField(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_row, Scaleform::GFx::Value& a_field,
		const std::string& a_printed, const std::string& a_text, bool a_mayShrink);

	// Form B: a_rate in a_cell, the empty field right of the number, at
	// the number's size and in its colour, aligned as the cell is. An
	// empty field's getTextFormat holds no font and no size, so the
	// format is the number's. False when it would not fit or would
	// shrink, with the cell left empty and a_why saying so in words.
	[[nodiscard]] bool WriteInCell(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_field, Scaleform::GFx::Value& a_cell,
		const std::string& a_rate, std::string& a_why);
}
