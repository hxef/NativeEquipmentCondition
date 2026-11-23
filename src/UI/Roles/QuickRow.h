#pragma once

#include "UI/Roles/Roles.h"

#include <optional>
#include <vector>

// What stands around a quick container row's CND at the row's right edge, and
// the fields a HUD mod draws the row's name in beside the row's own. Where the
// row's name starts and ends is in Hud.h.
namespace Roles::QuickRow
{
	// Whether a_box, in a_row's units, meets no component of the row or of
	// its list that shows now. Left out are text fields, which the name's
	// end in Hud.h or ColumnStart counts, NEC's own, whose names start with
	// NEC_, the list's other rows, each a clip holding a name field, and a
	// component behind the whole of a_room, the room the row gives its text,
	// as the row's highlight is. A clip holding others counts by its shown
	// children only, since it may keep hidden ones anywhere.
	[[nodiscard]] bool Clear(Value& a_row, const Box& a_box, const Box& a_room);

	// Whether a_field draws a_row's name: the row's own name field, or a
	// field whose text is a piece of that field's text. A HUD mod that hides
	// the name field and writes a long name as a title over a subtitle draws
	// it in such fields. NEC's own fields never count.
	[[nodiscard]] bool OfName(Value& a_row, Value& a_field);

	// The fields on screen in a_row other than its own name field that
	// draw its name, see OfName. They make room for the CND as the name
	// field does.
	[[nodiscard]] std::vector<Value> NameFields(Scaleform::GFx::Movie& a_movie, Value& a_row);

	// The left edge of the first text field on screen in a_row that shows
	// text right of a_from, in the row's units, and is neither NEC's nor
	// one that draws the name, as a count or weight column a HUD mod adds.
	// Such a field ends the room the row gives its name and CND. Nothing
	// when the row holds none.
	[[nodiscard]] std::optional<double> ColumnStart(Scaleform::GFx::Movie& a_movie, Value& a_row, double a_from);
}
