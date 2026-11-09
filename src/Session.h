#pragma once

#include "Core/Plugin.h"

// What NEC does as a game ends and the next begins, outside the feature
// table: it drops what play remembers of the last game and measures the load
// order again, at a save load, a new game, the main menu and a coc from there.
// Like main.cpp, it sits outside the ladder and knows every feature that
// keeps something of play.
namespace Session
{
	// Drops what play remembers of the last game.
	void ForgetPlay();

	// Reads the recipes, crafting perks, leveled lists and characters again.
	void MeasureAgain();

	// A save load or a new game begins, so no coc from the main menu is due.
	void Begins();

	// Watches the main menu and the loading screens, from the first time
	// game data has loaded. Later calls do nothing.
	void WatchMenus();
}
