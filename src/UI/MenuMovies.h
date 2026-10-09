#pragma once

#include "Core/Plugin.h"

// The plugin's one entry point for the game's menu movies. Every menu is a
// Flash movie, and F4SE calls a registered function with each movie as it
// loads. F4SE takes one registration per plugin, so this registers once and
// hands every movie to every feature that runs, see Feature.h.
namespace MenuMovies
{
	// Registers with F4SE, which only accepts Scaleform registrations while the
	// plugin loads.
	void Install();

	// Whether a movie's file name is a_name, ignoring case, since the game's
	// own file names are not consistent about it.
	bool IsMovie(std::string_view a_file, std::string_view a_name);
}
