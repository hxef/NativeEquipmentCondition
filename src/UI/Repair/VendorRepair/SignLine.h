#pragma once

#include "UI/Flash.h"
#include "UI/Repair/VendorRepair/Stock.h"
#include "UI/Roles/TraderSide.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The sign as 1 text field beside the trader's title, or left of the
// trader's frame where that row has no room, for a screen with no room or no
// panel under their list: "Repairs up to  Weapons 100%  Armor 80%", the same
// with short kind names where that is too wide, "Wpn 100%  Arm 80%", kinds of
// 1 level sharing 1 number, "Wpn/Arm 100%", or 2 rows with the full names
// where that is too wide too. Where the movie hides the engine's list
// background and draws the list's frame lines, a 1 line form can show in a
// frame of the same lines. Also what the framed panel shares with it: the
// field, the fonts, the colour and the lines. Private to this folder.
namespace VendorRepair::Sign
{
	// A form of the line, in the order LineOf makes them.
	struct Form
	{
		std::string_view          said;        // where the trace says the sign shows above the list
		std::string_view          saidLeft;    // and left of the frame
		std::string_view          saidFramed;  // and in its frame, for a 1 line form
		std::string               html;        // what the field shows
		Flash::Box                own;         // the field's box with it, in its own units
		std::optional<Flash::Box> frame;       // a 1 line form's 2 frame lines, in the field's units
	};

	// The frame is 1 Shape drawn in the field's units, so it goes where the
	// field goes, with the lines of 1 form at a time.
	struct Line
	{
		Scaleform::GFx::Value field;
		std::vector<Form>     forms;  // 1 line first, never empty
		Scaleform::GFx::Value frame;  // around the first, not a display object when the movie refuses the Shape
	};

	// The gap kept around the sign, in stage px.
	inline constexpr double GAP = 8.0;

	// The space under the line left of the trader's frame, in stage px.
	// Smaller than GAP there, so a button a movie shows just under that place
	// on some tabs only leaves the sign where it is.
	inline constexpr double GAP_UNDER_LEFT = 5.0;

	// The fonts of the list's rows and of its title: words in the first,
	// numbers and the panel's title in the bold one. Each language maps them
	// to its own letters.
	inline constexpr const char* WORD_FONT = "$MAIN_Font";
	inline constexpr const char* NUMBER_FONT = "$MAIN_Font_Bold";

	// The frame's hooks, 4 units long at each end of each line, as vanilla's
	// list frame draws them.
	inline constexpr double HOOK = 4.0;

	// Half a frame line counted into a box, in the side's units. The line is
	// 2 screen px wide at any scale, 1.33 units at 1080p.
	inline constexpr double STROKE = 1.0;

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

	// The framed line's spot, a_frame being the frame's box: the frame's
	// right end on the list frame's right end, its bottom GAP above the
	// frame's top. The field and the frame both go to it. Its box is the
	// frame's.
	[[nodiscard]] Spot FramedOf(const Flash::Box& a_frame, const Roles::TraderSide::Room& a_room);

	// The list frame's right end in the side's units: its frame lines' box
	// where they show, else the bracket's.
	[[nodiscard]] double RightEnd(const Roles::TraderSide::Room& a_room);

	// The line's forms for a screen, from LineOf.
	struct Forms
	{
		std::vector<Form> forms;
	};

	// A form of the line, whether it goes left of the frame, and whether
	// it is the 1 line form in its frame.
	struct Pick
	{
		std::size_t form = 0;
		bool        beside = false;
		bool        framed = false;
	};

	// The line's spot in a_pick's form.
	[[nodiscard]] Spot SpotFor(const Forms& a_line, const Pick& a_pick, const Roles::TraderSide::Room& a_room);

	// The line's form with room: a_held, the form shown last, while it still
	// fits where it is, so a title row that grows and shrinks back leaves the
	// sign as it is. Else above the list in LineOf's order, the full names in
	// 1 line, then the short names in 1 line, each in its frame where
	// a_framable and then plain, then 2 rows, and then the same plain forms
	// left of the frame. The rest of the screen is read only when the place
	// left of the frame is tried, and a_around says whether it was. Nothing
	// where no form has room.
	[[nodiscard]] std::optional<Pick> PickOf(const Forms& a_line, const std::optional<Pick>& a_held, bool a_framable, Roles::TraderSide::Side& a_side,
		Roles::TraderSide::Room& a_room, bool& a_around);

	// The size the title's letters show at in the side's units, from its
	// format and its scale. A hidden title still gives it. Nothing when the
	// movie has no title or its box does not read.
	[[nodiscard]] std::optional<double> TitleSize(Roles::TraderSide::Side& a_side);

	// A text field named a_name, sized to its text, in the title's drop
	// shadow, which the mouse passes through. Not a display object when the
	// movie refuses a write.
	[[nodiscard]] Scaleform::GFx::Value FieldOf(Scaleform::GFx::Movie& a_movie, Roles::TraderSide::Side& a_side, const char* a_name);

	// An empty Shape named a_name, or not a display object.
	[[nodiscard]] Scaleform::GFx::Value ShapeOf(Scaleform::GFx::Movie& a_movie, const char* a_name);

	// Draws the list's frame on a_shape with vanilla's own call, a white 2
	// px line that keeps its width at any scale: a top line along a_frame's
	// top with a hook down at each end, broken from a_gap's first to its
	// second across, and a bottom line with a hook up at each end. False when
	// the movie refuses a call.
	bool DrawFrame(Scaleform::GFx::Value& a_shape, const Flash::Box& a_frame, std::optional<std::pair<double, double>> a_gap = std::nullopt);

	// Draws a_form's frame on the line's frame Shape in place of the lines it
	// held. False when the movie refuses a call.
	bool Reframe(Scaleform::GFx::Value& a_shape, const Form& a_form);

	// The line for a trader who repairs a_ceilings: 1 text field with the
	// title's edge, at 3/5 of its size and never under 15, never shrunk to
	// fit, in white until ColorLine gives it the title's colour. Its forms:
	// 1 line, 1 line with short kind names where that is narrower, the title
	// over the kinds, then the kinds on both rows where that is narrower. 1
	// row is right aligned, 2 rows start at the left, so the title starts
	// over the kinds. "No repairs" is 1 line only. A hidden title still gives
	// its size, edge and colour. Nothing when the movie has no title or
	// refuses to make or fill the field.
	[[nodiscard]] std::optional<Line> LineOf(Scaleform::GFx::Movie& a_movie, Roles::TraderSide::Side& a_side, const Ceilings& a_ceilings);

	// Gives a_clip, the line, its frame or the panel, the colour the title's
	// letters show now. Asked at each placing, since a movie can colour its
	// title after the sign is made.
	void ColorLine(Scaleform::GFx::Value& a_clip, Roles::TraderSide::Side& a_side);

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
