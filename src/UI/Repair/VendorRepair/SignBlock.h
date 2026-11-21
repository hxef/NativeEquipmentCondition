#pragma once

#include "Core/Plugin.h"
#include "UI/Repair/VendorRepair/SignLine.h"
#include "UI/Repair/VendorRepair/Stock.h"
#include "UI/Roles/TraderSide.h"

#include <optional>
#include <string_view>

// The sign as a panel under the trader's list in the list's own frame style,
// see Sign.h: its title breaks the top frame line, as the list's title breaks
// the list's, a row per kind repaired follows with the word at the left and
// the percent at the right, then the bottom line. Behind them a back clip the
// engine paints the list's background over, see SignBack.h. Sized from the
// trader's title and its own text, never shrunk to fit. Private to this
// folder.
namespace VendorRepair::Sign
{
	struct Panel
	{
		Scaleform::GFx::Value clip;  // the panel, its own units the side's
		Scaleform::GFx::Value back;  // the clip the background target wraps
	};

	// The title over a row per kind repaired, or "No repairs" alone over 1
	// empty row's height, the title in capitals as the list's own, drawn
	// down from 0 with its left frame end at 0. Nothing when the movie
	// refuses a step, with a_missing naming the part it lacks for the trace.
	[[nodiscard]] std::optional<Panel> Build(Scaleform::GFx::Movie& a_movie, Roles::TraderSide::Side& a_side, const Ceilings& a_ceilings,
		std::string_view& a_missing);

	// The panel's spot: its right frame end on the list frame's right end,
	// its top GAP under the bracket. Nothing when its box does not read as
	// numbers.
	[[nodiscard]] std::optional<Spot> UnderOf(Scaleform::GFx::Value& a_panel, const Roles::TraderSide::Room& a_room);
}
