#pragma once

#include "Core/Plugin.h"

// The bench's item list and its mod slots, as an item too worn to modify
// needs them: listed even where no mod fits it, faded, and given no slots,
// see Display.h for how. Private to this folder.
namespace Workbench
{
	// Patches the 3 functions on the bench's table that fill its item list,
	// its slots and the mods behind a slot, and the call that hands the
	// rebuilt item list to Flash.
	void InstallLists();
}
