#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// How a row's meter, or its number at Percent, looks and where it sits on the
// row. Private to this folder.
namespace QuickContainer
{
	// The meter's size in the row's coordinates, where the name is 20 high:
	// the HP meter's drawing in small, a bar inside a 1 pixel frame with a
	// pixel of space between.
	constexpr double METER_WIDTH = 26.0;
	constexpr double METER_HEIGHT = 8.0;

	// The space before the meter, a little more than the 4 the row leaves
	// before its first icon.
	constexpr double METER_GAP = 6.0;

	// The height the row's icons are centred on, read off the
	// QuickContainerItem symbol in HUDMenu.swf.
	constexpr double METER_CENTER_Y = 14.0;

	// Draws a meter with its left edge at x 0 and its middle at y 0, its bar
	// filling from the frame's left side, or from its right side with
	// a_fromRight.
	void Draw(Scaleform::GFx::Value& a_meter, std::int32_t a_percent, bool a_selected, bool a_fromRight);

	// At Percent the row shows CND and its percent in the meter's place, in
	// the name's font at this share of the name's size, as the pickup
	// prompt's word is to the name over it.
	constexpr double NUMBER_SHARE = 0.7;

	// The font a row's name is written in, read from its field's own text
	// format: its name, such as $MAIN_Font, and its size.
	struct Letters
	{
		std::string font;
		double      size = 0.0;

		bool operator==(const Letters&) const = default;
	};

	// The letters of a_name, a row's name field, or nothing when it has no
	// text format to read.
	[[nodiscard]] std::optional<Letters> LettersOf(const Scaleform::GFx::Value& a_name);

	// Writes a_text into a row's number field in a_letters at NUMBER_SHARE of
	// their size, black on the highlighted row and white elsewhere, as the
	// row writes its name. Returns how wide its letters are in the row's
	// units, or nothing when they cannot be measured.
	std::optional<double> Write(Scaleform::GFx::Value& a_field, std::string_view a_text, const Letters& a_letters, bool a_selected);
}
