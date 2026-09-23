#pragma once

#include "Core/Plugin.h"

// The hit sink, and what a blow was. Private to this folder.
namespace WeaponEvents
{
	// Registers the sink that wears the player's weapon as a blow struck by
	// hand lands and logs everybody else's, once.
	void RegisterHitSink();
}
