#pragma once

#include "Core/Plugin.h"

// The meters on the HUD's rows. Private to this folder.
namespace QuickContainer
{
	// Reads how wide a row's name starts out, as the HUD movie loads and
	// whatever the switch says. Once the game draws an item in a row it
	// narrows the name by the item's icons, and the meters can be added later,
	// when the switch goes on.
	void ReadNameWidth(Scaleform::GFx::Movie& a_movie);

	// Adds a meter to each row of the HUD movie's quick container and the
	// listeners that keep them in sync with the rows. Returns false, with a
	// line in the log saying why, when the movie will not take them.
	bool AddMeters(Scaleform::GFx::Movie& a_movie);
}
