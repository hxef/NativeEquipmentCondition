#pragma once

#include "Core/Plugin.h"

// The hit sink, and what a blow was. Private to this folder.
namespace WeaponEvents
{
	// Registers the sink that wears a weapon as a blow struck by hand with it
	// lands, once.
	void RegisterHitSink();
}
