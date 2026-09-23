#pragma once

#include "Core/Plugin.h"

#include <span>
#include <string_view>

// One feature and the 4 points where the plugin calls it. Any may be left out.
//
// Features.cpp lists every feature in the order it runs. main.cpp walks the
// list forward for Install and Load and backward for Unload, and MenuMovies.cpp
// walks it for OnMovieLoaded. A row whose switch is off is passed over. A new
// feature is a namespace with these entry points and 1 row in Features.cpp.
struct Feature
{
	const char* name = nullptr;

	// The switch that turns the feature off, or nothing for one always on.
	// While it is off, none of the functions below run. Read as the game
	// starts, see Settings.h. A function another feature calls directly is not
	// covered and checks the setting itself, as Jam::Roll does.
	const REX::TIniSetting<bool>* on = nullptr;

	// Patches code, once, while the plugin loads. No game data exists yet, but
	// what the executable holds is there from the start, such as its function
	// tables and its console commands.
	void (*Install)() = nullptr;

	// Reads the load order, every time game data has loaded: at the start and
	// after every full reset.
	void (*Load)() = nullptr;

	// Releases every form Load kept, just before the game deletes them all.
	void (*Unload)() = nullptr;

	// Adds to a menu movie. Runs for every movie the game loads and checks the
	// file name itself, see MenuMovies::IsMovie.
	void (*OnMovieLoaded)(Scaleform::GFx::Movie& a_movie, std::string_view a_file) = nullptr;

	[[nodiscard]] bool IsOn() const { return !on || on->GetValue(); }
};

// Every feature, in the order they run. Defined in Features.cpp.
[[nodiscard]] std::span<const Feature> Features();
