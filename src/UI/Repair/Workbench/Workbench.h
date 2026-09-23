#pragma once

#include "Core/Plugin.h"

#include <string_view>

// Repairing a worn weapon at the weapon workbench. A weapon below full cannot
// be modified: its row is faded, it gets no mod slots, the heading over them
// goes, and asking for them says why. Instead a REPAIR button on the bar asks
// how far to repair the gun, a level at a time, then puts the game's own
// crafting confirmation up with the components on it. Saying yes spends them
// and pays the experience crafting a mod from them would. Wear below 5% is
// repaired on the spot for free, since a floor at full would otherwise make one
// shot a trip to the bench.
//
// The button is the only way in. The game shares one button between REPAIR and
// RENAME, so the flag picking between them is written on every change of
// highlight.
//
// Bench.h holds the basics, Cost.h what a repair costs, Job.h the repair from
// the question to the finished gun, and Display.h how a worn weapon is shown
// and kept out of the slots.
namespace Workbench
{
	// Patches the workbench menu's function table and its confirmation
	// callback's.
	void Install();

	// Logs the language the bench speaks, once the game has read it.
	void Load();

	// Adds the listener that fades the weapon in hand to the bench movie and
	// ignores every other.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);
}
