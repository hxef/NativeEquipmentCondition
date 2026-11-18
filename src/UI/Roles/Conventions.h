#pragma once

// Method and member names some menu movies answer beyond the vanilla ones.
// The one place for them, so a movie with a bar or list columns of its own
// costs a row here and nothing in a feature. Each is a method or a member,
// never a mod's name. Private to this folder.
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

	// A movie class whose static settings name the item card rows that get a
	// list column each, as 1 comma list. Read anew at every list build.
	struct Columns
	{
		const char* settings;  // the class that holds the movie's settings
		const char* list[3];   // its members down to the comma list of card rows that get a column each
		const char* loaded;    // runs a function once those settings have loaded
	};

	inline constexpr Columns COLUMNS[]{
		{ "M8r.Mods", { "config", "Columns", "sColumnsForceKeysToOwnCol" }, "addOnConfigLoaded" },  // seen on FallUI Inventory 2.2.1's and FallUI Workbench 1.4.1's lists
	};

	// The message box mod in MessageBox.h is FallUI's Confirm Boxes. DEF_UI's
	// box has no style, so it is left as it is.
}
