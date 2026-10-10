#pragma once

#include "Core/Plugin.h"

#include <string_view>

// Repairing a worn weapon or piece of armor at its workbench. An item below
// full cannot be modified: its row is faded, it gets no mod slots, the heading
// over them goes, and asking for them says why. Instead a REPAIR button on the
// bar asks how far to repair the item, a level at a time, then puts the game's
// own crafting confirmation up with the components on it, or the power armor
// station's list of them where the bench is short. Saying yes spends them and
// pays the experience crafting a mod from them would. Slight wear is repaired
// on the spot for free, see FreeAbove in Bench.h, since a floor at full would
// otherwise make one shot a trip to the bench. An item the bench has nothing to
// rebuild from, see Cost.h, is sent to a trader. A worn item the bench would
// leave out of its list, since no mod fits it, is listed all the same and
// stays greyed, see Display.h.
//
// The weapon and armor benches are one menu class with different lists, so one
// set of hooks serves both. The button is the only way in. The game shares one
// button between REPAIR and RENAME, so the flag picking between them is
// written on every change of highlight.
//
// Bench.h holds the basics, Cost.h what a repair costs, Job.h the repair from
// the question to the finished item, Box.h its confirmation and what yes takes,
// Missing.h what a repair the bench cannot pay for shows, Label.h the button's
// word and the mods heading, Display.h how a worn item is shown and kept out of
// the slots, and Lists.h the hooks on the lists that do it.
namespace Workbench
{
	// Patches the workbench menu's function table, each place by itself.
	void Install();

	// Logs the language the bench speaks, once the game has read it.
	void Load();

	// Forgets what the last bench listed and the word its button read, and
	// adds the listener that fades the equipped items and keeps that word to
	// the bench movie. Ignores every other movie.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);

	// Whether the bench repairs: the 3 places behind the REPAIR button, the
	// flag, the grey and the press, all still run NEC's hooks. The mod lock
	// and the perk text ask it too, so they never outlive the repair.
	[[nodiscard]] bool Repairs();
}
