#pragma once

#include "Core/Plugin.h"

// A CND bar in the pickup prompt, the name and "E) Take" the HUD shows while
// the crosshair rests on something lying around. For a weapon or a piece of
// armor it shows the condition, and for food, while food spoils, how fresh it
// is: full when fresh, empty when spoiled. It sits under the name, centred on
// it, in the gap above the button hints, drawn as the other HUD bars are, see
// HudParts::Readout. It has a colour target of its own, so it is drawn whole
// in the gameplay HUD colour, and in the warning colour with the name and the
// hints while taking the item is stealing, see Bar.cpp.
//
// What the game sends to the HUD names no item, only its text, so the call
// that builds the prompt is patched: after it, the crosshair's pick says
// which item the prompt stands for. A loose item no holder has rolled yet
// rolls on that first look, as the player's pickup would roll it, see
// SpawnCondition::RollOnLook, so the bar shows what the pickup gives. A
// trader's stock, gear a faction owns, rolls when it is bought and shows no
// bar until then. The HUD draws the bar a frame later, and only while the
// prompt prints the name that was built.
//
// Look.cpp is the prompt as the game builds it, Bar.cpp the bar in the HUD's
// prompt. Look.h and Bar.h are what they share.
namespace PickupBar
{
	// Patches the call that builds the prompt.
	void Install();

	// Adds the bar to the HUD movie through a HudParts::Waiter. Ignores every
	// other movie.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);
}
