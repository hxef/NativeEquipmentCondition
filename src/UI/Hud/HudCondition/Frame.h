#pragma once

#include "Core/Plugin.h"

// The frame listener on the readout. Every HUD frame it follows the counter,
// lays the bar out at its spot and size, places it, draws the level and fades
// it. Private to this folder.
namespace HudCondition
{
	// The listener for the readout's enterFrame event.
	[[nodiscard]] Scaleform::GFx::FunctionHandler& Frames();

	// Forgets all the listener drew, for a new HUD movie.
	void ResetFrames();
}
