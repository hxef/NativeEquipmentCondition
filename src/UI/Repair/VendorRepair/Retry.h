#pragma once

#include "Core/Plugin.h"

// Waiting for a button bar on the barter screen, and watching it after.
// Private to this folder.
//
// A UI replacer can build its bars after the last highlight NEC hears as the
// screen opens. So a refresh that finds no bar REPAIR can join runs again each
// frame for 2 s, then gives up on that screen until the next one opens. C
// repairs all the while, since the button is built before any bar is looked
// for.
//
// A UI replacer can also hand its bar a fresh list without REPAIR after REPAIR
// joined one, once its own settings have loaded. So once REPAIR first joins a
// bar on a screen, the refresh runs each frame for 3 s more whatever it finds,
// and puts REPAIR back on the new list.
namespace VendorRepair
{
	// Starts the wait on a_menu's movie. Does nothing while a wait runs there,
	// so its deadline never moves, or on a screen whose wait ran out.
	void WaitForBar(RE::BarterMenu& a_menu);

	// Ends a wait that runs. a_found says REPAIR is on a bar, for the trace.
	// A watch keeps running.
	void EndWait(bool a_found);

	// Starts the watch on a_menu's movie, once per screen, so its deadline
	// never moves.
	void WatchBar(RE::BarterMenu& a_menu);

	// Ends any wait or watch and forgets a screen given up on or watched, for
	// a new screen.
	void ForgetWait();
}
