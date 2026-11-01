#pragma once

#include "Core/Plugin.h"

// The bar in the HUD's pickup prompt. Private to this folder.
namespace PickupBar
{
	// Adds the bar and its frame listener to the HUD movie's pickup prompt.
	// Says in NEC.log why when the movie will not take it.
	void AddBar(Scaleform::GFx::Movie& a_movie);
}
