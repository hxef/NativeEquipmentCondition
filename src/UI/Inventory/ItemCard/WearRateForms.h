#pragma once

#include "Core/Plugin.h"

#include <optional>
#include <string>

// The 2 forms of WriteWearRate in WearRate.cpp that measure the CND row: the
// rate in the number's own field, and in the cell beside it, and the write
// that turns a change the movie printed beside it. The CND row's bar takes
// its measures from here too. Private to this folder.
namespace ItemCard
{
	// At least this much stays clear between the label's last letter and
	// the number's field, in the row's units, about a space and a half on
	// the vanilla card.
	inline constexpr double LABEL_GAP = 8.0;

	// The size a_field's text has, from its text format. Nothing when it
	// has none to read.
	[[nodiscard]] std::optional<double> FontSize(Scaleform::GFx::Value& a_field);

	// How wide a_text is in a_field's text format at its natural size, in
	// the field's own units. Nothing when the movie refuses to measure it.
	[[nodiscard]] std::optional<double> NaturalWidth(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_field, const std::string& a_text);

	// Form A: a_text, the percent and the rate, in the number's own field
	// at its natural size. The field grows left with its right edge kept,
	// only while the row shows nothing but its background in that stretch,
	// and its text starts no further left than the label's last letter and
	// LABEL_GAP, wherever the field's box starts. The text is taken as
	// right aligned, as vanilla's number is, which puts it furthest left.
	// How much wider the field grew, or nothing when it would not fit, with
	// the field as it was.
	[[nodiscard]] std::optional<double> WriteInField(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_row, Scaleform::GFx::Value& a_field,
		const std::string& a_printed, const std::string& a_text);

	// Form B: a_rate in a_cell, the empty field right of the number, at
	// the number's size and in its colour, aligned as the cell is. An
	// empty field's getTextFormat holds no font and no size, so the
	// format is the number's. False when it would not fit or would
	// shrink, with the cell left empty and a_why saying so in words.
	// a_keepColour writes over a change the movie printed in the cell: the
	// rate takes the colour the movie gave that change, the number's tint
	// is not copied, and a try that fails leaves the movie's text as it was.
	[[nodiscard]] bool WriteInCell(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_field, Scaleform::GFx::Value& a_cell,
		const std::string& a_rate, bool a_keepColour, std::string& a_why);

	// Writes a_text into a_field in the font, size and colour its text has
	// now, so a change the movie printed keeps its look. The movie's own
	// shrink of that field stays in charge of its size. False when the
	// field has no look to read, with nothing written.
	bool WriteKeepingLook(Scaleform::GFx::Value& a_field, const std::string& a_text);
}
