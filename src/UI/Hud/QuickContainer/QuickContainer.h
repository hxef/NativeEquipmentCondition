#pragma once

#include "Core/Plugin.h"

// A CND meter on the rows of the quick container, the short list the HUD shows
// while the crosshair rests on a container, a companion, a power armor frame or
// a workbench. Each row is a name, a count and a few icons. A row for an item
// that wears gets a meter after its icons, drawn like the HUD's HP meter, a
// thin frame around a bar, black on the highlighted row.
//
// The hard part is which stack a row stands for, since 3 rows can all read 10mm
// Pistol. What the game sends to the HUD keeps only the name, the count and the
// icons, so 2 calls that build the rows are patched: one notes each row's
// condition as the game makes it, the other hands the finished list over. The
// HUD draws on its own thread a moment later, so the last few lists are kept
// and each row takes its meter from the newest whose names and counts match.
//
// Rows.cpp is the rows as the game builds them, and Meters.cpp the meters on
// the HUD's rows. Rows.h and Meters.h are what they share.
namespace QuickContainer
{
	// Patches the calls that build the rows. Call it while the plugin loads.
	void Install();

	// Adds the meters to the HUD movie and ignores every other movie.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);
}
