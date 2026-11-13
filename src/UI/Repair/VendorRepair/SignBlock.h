#pragma once

#include "Core/Plugin.h"
#include "UI/Repair/VendorRepair/Stock.h"

#include <string_view>

// The sign as a block under the trader's list, made of the movie's own item
// card rows at the card's scale, see Sign.h. Private to this folder.
namespace VendorRepair::Sign
{
	// The title over a row per kind repaired, or "No repairs" alone,
	// stacked down from 0 and scaled to a_scale. Not a display object
	// when the movie refuses a step, with a_missing naming the part it
	// lacks for the trace.
	[[nodiscard]] Scaleform::GFx::Value Build(Scaleform::GFx::Movie& a_movie, const Ceilings& a_ceilings, double a_scale, std::string_view& a_missing);
}
