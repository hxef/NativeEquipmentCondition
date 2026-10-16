#pragma once

#include "Core/Plugin.h"

// Loot lying in the world. A weapon or piece of armor the game places gets a
// mark, UNROLLED in Condition.h, and its first holder rolls it, see HandOver
// in Handover.h. Private to this folder.
namespace SpawnCondition
{
	// Patches the 4 sets of Loot lying around: a new reference and a drop, a
	// world item the game picks again from its leveled list, an item placed
	// to spawn inside a container, and the 2 calls that read a loose item's
	// condition for scripts and the console. Says so in the log.
	void InstallWorld();
}
