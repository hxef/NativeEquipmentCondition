#pragma once

#include "Core/Plugin.h"

#include <cstdint>

// How a row's meter looks and where it sits on the row. Private to this folder.
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

	// Draws a meter with its left edge at x 0 and its middle at y 0.
	void Draw(Scaleform::GFx::Value& a_meter, std::int32_t a_percent, bool a_selected);
}
