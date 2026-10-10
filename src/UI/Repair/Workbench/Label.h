#pragma once

#include "Core/Plugin.h"

#include "UI/Repair/Workbench/Bench.h"

// What the bench says about the item it shows: REPAIR, MEND or RENAME on the
// button, and whether the CURRENT MODS heading stays. Private to this folder.
namespace Workbench
{
	// Says what the bench is looking at: whether the button offers REPAIR or
	// RENAME, and whether the CURRENT MODS heading still applies. Both follow
	// the item, so both are written wherever it or its condition changes.
	void Announce(RE::ExamineMenu* a_menu, const Selection& a_selection);

	// Gives the bench's REPAIR button its word on every bar the menu has:
	// MEND over an item worn so little that repairing it is free, REPAIR
	// otherwise. Called as the buttons redraw, when the bar is sure to hold
	// the bench's own.
	void Label(RE::ExamineMenu* a_menu, const Selection& a_selection);

	// Writes the word Label last chose again on the engine's list of the
	// bench showing a_movie, since a movie can hand its bar a fresh list of
	// the same buttons later. Does nothing before Label has chosen a word on
	// this movie, or while repairs are off, so the game's own REPAIR stands.
	// Called every frame.
	void Relabel(Scaleform::GFx::Movie& a_movie);

	// Forgets the word, for a new bench movie.
	void ForgetWord();
}
