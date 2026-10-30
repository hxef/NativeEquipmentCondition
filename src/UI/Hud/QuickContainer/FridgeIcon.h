#pragma once

#include "Core/Plugin.h"

// The fridge icon beside the container's name at the top of the quick
// container, while the crosshair rests on a container on the fridge list and
// FoodSpoil::FridgeIconOn says so. It is Fallout 4's cryo damage icon, a
// snowflake, made at run time from a file the game ships it in. Private to
// this folder.
namespace QuickContainer
{
	// Forgets the icon of the last HUD movie, as each HUD movie loads.
	void ForgetFridgeIcon();

	// Starts loading the icon into the HUD movie. Once it arrives, it goes
	// beside the name with a listener that shows it, places it and hides it
	// every frame. Logs why when the movie will not take it.
	void AddFridgeIcon(Scaleform::GFx::Movie& a_movie);
}
