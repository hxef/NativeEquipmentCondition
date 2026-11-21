#pragma once

#include "Core/Plugin.h"
#include "UI/Repair/VendorRepair/SignLine.h"
#include "UI/Roles/TraderSide.h"

#include <optional>
#include <string_view>

// The sign's forms on the barter screen, see Sign.h: the clips Draw keeps for
// Place, the form the screen gets and its own place, and how a form goes in
// and out. A form not shown is taken out of the display list, never only
// hidden, so the side's colour never stretches over another part, and the
// panel's background goes with it. Private to this folder.
//
// Left of the frame the line stays a child of the side, since the engine
// draws a part only inside the box of a part it colours, and the side's box
// grows to hold it. That box draws no background, the brackets have their
// own, and over the gap it meets only parts the engine colours in the same
// HUD colour, as the item card and the caps transfer box.
namespace VendorRepair::Sign
{
	// How the trace says a target goes.
	inline constexpr auto COMES_OUT = "as it comes out"sv;
	inline constexpr auto CLOSES = "as the screen closes"sv;

	// The panel, its back clip, the line and the line's frame, each not a
	// display object where Draw kept none.
	struct Clips
	{
		Scaleform::GFx::Value sign;
		Scaleform::GFx::Value back;
		Scaleform::GFx::Value line;
		Scaleform::GFx::Value frame;
	};

	[[nodiscard]] Clips ClipsOf(RE::BarterMenu& a_menu);

	// Keeps a_clips on a_menu while they are out of the display list, and
	// the line's forms for this screen.
	void Keep(RE::BarterMenu& a_menu, const Clips& a_clips, Forms a_line);

	// Takes every clip Keep kept out and forgets them with the line's forms.
	void Clear(RE::BarterMenu& a_menu);

	// Takes a_clip out of the display list, if it is in.
	void Off(Scaleform::GFx::Value& a_clip);

	// Takes the panel out with its background, a_how saying why.
	void PanelOff(RE::BarterMenu& a_menu, Clips& a_clips, std::string_view a_how);

	// Says where the sign is now, once per change, and forgets it for a
	// screen opening or closing.
	void Say(RE::BarterMenu* a_menu, std::string_view a_where);
	void ForgetSaid();

	// A form and its own place: the panel, or the line in a_pick's form.
	struct Choice
	{
		std::optional<Pick> pick;  // nothing for the panel
		Spot                spot;
	};

	// The form the screen gets, by its own rules, and its own place: the
	// panel under the list where the screen can show it and it ends above
	// the bar's row, else the line's form PickOf finds. Nothing where no
	// form has room.
	[[nodiscard]] std::optional<Choice> Choose(Roles::TraderSide::Side& a_side, Roles::TraderSide::Room& a_room, Clips& a_clips, bool& a_around);

	// The first form in Choose's order the screen can show, at its own
	// place whether it has room there or not: the panel, else the framed
	// line, else the 1 line form above the list. Nothing where the screen
	// can show none.
	[[nodiscard]] std::optional<Choice> FirstOf(Roles::TraderSide::Room& a_room, Clips& a_clips);

	// Puts a_choice's form in a_into, the side or the moved sign's clip, at
	// its spot and takes the rest out. Gives where the trace says the form
	// shows, or nothing when the line's field refuses its form's text.
	std::optional<std::string_view> Show(RE::BarterMenu& a_menu, Roles::TraderSide::Side& a_side, Clips& a_clips, const Choice& a_choice,
		Scaleform::GFx::Value& a_into);

	// Takes every form out, as none shows, and says why. False for a screen
	// with no line clip, where the title row is not in play.
	bool LeftOut(RE::BarterMenu& a_menu, const Roles::TraderSide::Room& a_room, Clips& a_clips);
}
