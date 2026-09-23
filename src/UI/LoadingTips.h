#pragma once

#include "Core/Plugin.h"

// The plugin's own tips on the loading screen. Every time a loading screen
// opens, the game collects the loading screen records whose conditions pass and
// picks one at random, screens with conditions first. The tips are added to
// that list, each a loading screen of the plugin's own with no conditions, the
// words of the tip and the picture of a vanilla screen it borrows, so a tip
// shows exactly as often as any one ordinary vanilla screen and never on the
// way out to the main menu.
//
// The tips are not records, since a record needs a plugin file. Each is made
// the way the game makes any loading screen, then handed its dynamic number
// back, which takes it out of the table the game looks forms up in, and given a
// number the game never hands out, one below FF000800. So nothing in the game
// can look it up and no save can point at it. The loading screen prints the
// number's last 4 digits as its catalogue number.
//
// Each tip is made once, the first time game data loads, and kept until the
// game shuts down. A tip about a feature switched off in NEC.ini is never made.
// Its picture belongs to the vanilla screen, which the game deletes on a full
// reset, so the tips hand their pictures back just before and borrow them again
// after.
//
// Adding them takes one patched call, where the loading screen collects
// everything it may show. The tips are in LoadingTips.cpp, their words in
// Text.cpp.
namespace LoadingTips
{
	// Patches the call, and with the trace logs on, the loading menu's
	// messages, to log each pick.
	void Install();

	// Makes and numbers a loading screen for every tip the first time, and
	// lends each the picture it borrows, logging a picture that is not there.
	void Load();

	// Hands every borrowed picture back.
	void Unload();
}
