#pragma once

#include "Core/Plugin.h"
#include "UI/Flash.h"
#include "UI/Repair/VendorRepair/Stock.h"
#include "UI/Roles/TraderSide.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The sign as 1 text field beside the trader's title, or left of the
// trader's frame where that row has no room, for a screen with no room or no
// block under their list: "Repairs up to  Weapons 100%  Armor 80%", or 2 rows
// of it where 1 line is too wide. Private to this folder.
namespace VendorRepair::Sign
{
	// A form of the line, in the order Place tries them.
	struct Form
	{
		std::string_view said;    // where the trace says the sign shows above the list
		std::string_view beside;  // and left of the frame
		std::string      html;    // what the field shows
		Flash::Box       own;     // the field's box with it, in its own units
	};

	struct Line
	{
		Scaleform::GFx::Value field;
		std::vector<Form>     forms;  // 1 line first, never empty
	};

	// The gap kept around the sign, in stage px.
	inline constexpr double GAP = 8.0;

	// The space under the line left of the trader's frame, in stage px.
	// Smaller than GAP there, so a button a movie shows just under that place
	// on some tabs only leaves the sign where it is.
	inline constexpr double GAP_UNDER_LEFT = 5.0;

	struct Spot
	{
		double     x = 0.0;
		double     y = 0.0;
		Flash::Box box;  // what it draws, in the side's units
	};

	// A form's spot: the field's right edge on the bracket's, or with
	// a_beside a gap left of the bracket's left edge, and its bottom a little
	// above the frame's top, so its lowest row sits in the same place in
	// every form. Its box is the text inside the field's margin.
	[[nodiscard]] Spot SpotOf(const Form& a_form, const Roles::TraderSide::Room& a_room, bool a_beside);

	// The line for a trader who repairs a_ceilings: 1 text field with the
	// title's edge, at 3/5 of its size and never under 15, never shrunk to
	// fit, in white until ColorLine gives it the title's colour. Its forms:
	// 1 line, the title over the kinds, then the kinds on both rows where
	// that is narrower. "No repairs" is 1 line only. A hidden title still
	// gives its size, edge and colour. Nothing when the movie has no title or
	// refuses to make or fill the field.
	[[nodiscard]] std::optional<Line> LineOf(Scaleform::GFx::Movie& a_movie, Roles::TraderSide::Side& a_side, const Ceilings& a_ceilings);

	// Gives the line the colour the title's letters show now. Asked at each
	// placing, since a movie can colour its title after the line is made.
	void ColorLine(Scaleform::GFx::Value& a_line, Roles::TraderSide::Side& a_side);

	// What can change the room beside the trader's title or the line's
	// colour with no new highlight: the title's text and colour, whether its
	// header shows and how wide it is, and how many parts the side holds. A
	// few reads, so it is cheap to ask each frame. A part that shows, hides
	// or moves there while the count stays is seen only at the next
	// highlight, and so is a colour the movie gives the title's text through
	// its format, which leaves textColor as it was. With a_each, for the place
	// left of the frame, also whether each part of the side shows, since a
	// movie can show a part beside the frame with no highlight, as a button
	// a moment after a tab change. A part of the rest of the screen that
	// shows alone there is still seen only at the next highlight, which
	// changes nothing while that part keeps GAP_UNDER_LEFT under the line,
	// as a tab's button under that place.
	struct TitleRow
	{
		std::string                      title;
		double                           shown = 0.0;
		double                           width = 0.0;
		double                           parts = 0.0;
		double                           color = 0.0;
		std::optional<std::vector<bool>> each;

		bool operator==(const TitleRow&) const = default;
	};

	[[nodiscard]] TitleRow RowOf(Roles::TraderSide::Side& a_side, bool a_each);
}
