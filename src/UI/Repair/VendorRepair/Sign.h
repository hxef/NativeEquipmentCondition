#pragma once

#include "Core/Plugin.h"
#include "UI/Roles/Bars.h"

#include <optional>

// The sign on the barter screen that shows how far the trader repairs each
// kind, in 1 of 2 forms, as a child of the trader's side, so it moves with
// that side and the engine colours it with the side. Private to this folder.
//
// The block goes under the trader's list: "Repairs up to" over 1 row per kind,
// "Weapons 100%", or "No repairs" alone, made of the movie's own item card
// rows at the card's scale. Its right edge is on the bracket's, its top 8
// stage px under it.
//
// The line goes above the list where the block has no room or the movie
// cannot build it: the same words in 1 text field right of the trader's
// title, ending on the bracket's right edge, see SignLine.h. Too wide for 1
// line, it takes 2 rows. Where that row holds none of these forms, the line
// goes left of the trader's frame in the same forms and order, its rows at
// the same height, ending a gap left of the bracket's left edge.
//
// A form shows only where the screen holds all of it and a gap of 8 stage px
// around it meets nothing the side or the bar draws, and left of the frame
// nothing else the screen draws either, else the sign is left out. Left of
// the frame the gap under it is 5, see SignLine.h. A form not shown is taken
// out of the side, never only hidden, so the side's colour never stretches
// over another part.
//
// Left of the frame the line stays a child of the side, since the engine
// draws a part only inside the box of a part it colours, and the side's box
// grows to hold it. That box draws no background, the brackets have their
// own, and over the gap it meets only parts the engine colours in the same
// HUD colour, as the item card and the caps transfer box.
namespace VendorRepair::Sign
{
	// Builds the block and the line again for a trader's side just rebuilt,
	// each where the movie allows it, kept off the screen until Place puts
	// one there. With Trader repairs or the sign switched off it only takes
	// the last one out, and counts nothing.
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

	// Whether the row over the trader's list on a_movie changed since Place
	// last tried the line there, so the refresh must run again. A movie can
	// change that row without moving the highlight, when a tab's title grows
	// or a part shows there. Asked each frame: a few reads while the line
	// shows or waits for room, 1 more per part of the side while the place
	// left of the frame is in play, and false at once while the block shows.
	[[nodiscard]] bool TitleRowChanged(const Scaleform::GFx::Movie* a_movie);
}
