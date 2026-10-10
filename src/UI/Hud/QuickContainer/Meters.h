#pragma once

#include "Core/Plugin.h"

// The meters on the HUD's rows. Private to this folder.
namespace QuickContainer
{
	// Forgets every meter and name width of the last HUD movie, as each HUD
	// movie loads and whatever the switch says, since the meters can be added
	// later, when the switch goes on.
	void ForgetMeters();

	// Adds a meter to each row of the HUD movie's quick container and the
	// listeners that keep them in sync with the rows. Returns false, with a
	// line in the log saying why, when the movie will not take them.
	bool AddMeters(Scaleform::GFx::Movie& a_movie);
}
