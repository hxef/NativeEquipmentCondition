#pragma once

// Method names some menu movies answer beyond the game's own. The one place
// for them, so a movie with a bar of its own costs a row here and nothing in a
// feature. Each is a method, never a mod's name. Private to this folder.
namespace Roles::Conventions
{
	struct Bar
	{
		const char* hints;   // returns the bar's list of hints
		const char* redraw;  // draws the bar again after a change
	};

	inline constexpr Bar BARS[]{
		{ "getButtonHints", "redraw" },  // seen on FallUI Inventory 2.2.1's button bar
	};

	// The message box mod in MessageBox.h is FallUI's Confirm Boxes. DEF_UI's
	// box has no style, so it is left as it is.
}
