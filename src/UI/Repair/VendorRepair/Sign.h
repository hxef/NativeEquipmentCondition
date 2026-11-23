#pragma once

#include "UI/Roles/Bars.h"

#include <optional>

// The sign on the barter screen that shows how far the trader repairs each
// kind, as a child of the trader's side, so it moves with that side and the
// engine colours it with the side. How a form goes in and out is in
// SignPlace.h, and the place the player moves it to in SignMove.h. Private to
// this folder.
//
// The panel goes under the trader's list in the list's own frame style:
// "REPAIRS UP TO" in its top frame line, then 1 row per kind, "Weapons 100%",
// then its bottom line, or "NO REPAIRS" over 1 empty row, with the engine's
// own see through list background behind it, see SignBlock.h and SignBack.h.
// Its right frame end is on the list frame's, its top 8 stage px under the
// bracket, and it ends above the bar's row. It shows only where the side
// shows the engine's background and the list's frame lines, and while NEC
// holds the barter screen's delete, which drops the background target.
//
// The line goes above the list where the panel has no room or the screen
// cannot show it: the same words in 1 text field right of the trader's
// title, ending on the bracket's right edge, see SignLine.h. Where the movie
// hides the engine's background and draws the list's frame lines, a 1 line
// form first tries a frame of those lines around it, its bottom line 8 stage
// px above the list's frame. Too wide for 1 line, it takes short kind names,
// "Wpn 100%", kinds of 1 level sharing 1 number, "Wpn/Arm 100%", and too wide
// for that, 2 rows with the title at the left. Where that row holds none of
// these forms, the line goes left of the trader's frame in the same plain
// forms and order, its rows at the same height, ending a gap left of the
// bracket's left edge. A form shows only where the screen holds all of it
// and a gap of 8 stage px around it meets nothing the side or the bar draws,
// and left of the frame nothing else the screen draws either, else the sign
// is left out. Left of the frame the gap under it is 5, see SignLine.h.
namespace VendorRepair::Sign
{
	// Builds the panel and the line again for a trader's side just rebuilt,
	// each where the movie allows it, kept off the screen until Place puts
	// one there. With Trader repairs, the sign or bNecOn switched off it only
	// takes the last one out, and counts nothing.
	void Draw(RE::BarterMenu* a_menu);

	// Puts the sign in the first place that has room, a_bar being the bar
	// REPAIR is on or would join, or takes it out where none has. Asked at
	// every refresh, since a bar of the movie's own can wrap to a 2nd line
	// when REPAIR comes or goes, and a tab's title can grow. It stays out
	// while no bar answers, since a movie can lay its bar out late.
	void Place(RE::BarterMenu* a_menu, const std::optional<Roles::Bars::Hints>& a_bar);

	// Takes the sign out and forgets what the trace was told, for a screen
	// opening or closing, since the next screen can reuse the movie.
	void Forget(RE::BarterMenu* a_menu);

	// Whether the panel or the line is on the barter screen now, so the
	// repair question leaves out what the sign already says.
	[[nodiscard]] bool Shown(RE::BarterMenu* a_menu);

	// Whether the row over the trader's list on a_movie changed since Place
	// last tried the line there, so the refresh must run again. A movie can
	// change that row without moving the highlight, when a tab's title grows
	// or a part shows there. Asked each frame: a few reads while the line
	// shows or waits for room, 3 more reads per part of the side while the
	// place left of the frame is in play, and false at once while the panel
	// shows.
	[[nodiscard]] bool TitleRowChanged(const Scaleform::GFx::Movie* a_movie);
}
