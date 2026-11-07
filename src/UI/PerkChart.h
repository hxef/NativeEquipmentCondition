#pragma once

#include "Core/Plugin.h"

// Every line of a perk's description on the level up perk chart. The chart's
// description field is 2 lines high and never shrinks its letters, so on its
// own it drops every word past the 2nd line of a longer description, such as a
// crafting perk's with NEC's line after it, while the card still leaves room
// for those lines. The field is made to size itself to its text, into that
// room.
namespace PerkChart
{
	// Joins the chart's movie as it loads.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);
}
