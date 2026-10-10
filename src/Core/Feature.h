#pragma once

#include "Core/Plugin.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Parts.h"
#include "Core/Settings.h"

#include <span>
#include <string_view>

// One feature and the 4 points where the plugin calls it. Any may be left out.
//
// Features.cpp lists every feature in the order it runs. main.cpp walks the
// list forward for Install and Load and backward for Unload, and MenuMovies.cpp
// walks it for OnMovieLoaded. Every walk calls every row whatever its switch
// says. Only a row whose switch is left to another mod is skipped, by Install
// and OnMovieLoaded, see Runs. A new feature is a namespace with these entry
// points and 1 row in Features.cpp.
struct Feature
{
	const char* name = nullptr;

	// The switch this feature's hooks ask on every call, or nothing for one
	// always on. While it is off the hooks give the game's own result.
	Settings::Live<bool>* on = nullptr;

	// The part this row's places belong to unless a patch call names
	// another, see CallPatch.h. Of the rows on 1 switch, only the 1 with a
	// part names the part the switch stands for, see PartOf. Every row that
	// patches has one.
	Part part = Part::kNone;

	// The part this row's switch rides on. When another mod takes a place of
	// it, this row's switch goes off with it, see Lose in Ledger.cpp.
	Part needs = Part::kNone;

	// Patches code, once, after every plugin has loaded and before any game
	// thread runs. No game data exists yet, but what the executable holds is
	// there from the start, such as its function tables and its console
	// commands.
	void (*Install)() = nullptr;

	// Reads the load order, every time game data has loaded: at the start and
	// after every full reset.
	void (*Load)() = nullptr;

	// Releases every form Load kept, just before the game deletes them all.
	void (*Unload)() = nullptr;

	// Adds to a menu movie. Runs for every movie the game loads and checks the
	// file name itself, see MenuMovies::IsMovie.
	void (*OnMovieLoaded)(Scaleform::GFx::Movie& a_movie, std::string_view a_file) = nullptr;

	// False once the part its switch stands for is left to another mod, or
	// the part it rides on. Such a row is not installed and adds nothing to a
	// movie. Load and Unload run for it anyway.
	[[nodiscard]] bool Runs() const { return !on || !CallPatch::IsYielded(*on); }
};

// Every feature, in the order they run. Defined in Features.cpp.
[[nodiscard]] std::span<const Feature> Features();

// The part a switch stands for: the part of the 1 row with that switch and
// a part. kNone for any other setting.
[[nodiscard]] Part PartOf(const Settings::Named& a_switch);
