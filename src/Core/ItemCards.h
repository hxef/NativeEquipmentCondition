#pragma once

#include "Core/Plugin.h"

// The Pip-Boy builds an item's card once and keeps it, so a weapon that wore
// down would keep printing the damage it had. The game's own equip handler
// rebuilds a whole category, and this calls that rebuild.
namespace ItemCards
{
	// Rebuilds the cards of one category, such as every weapon. Safe with the
	// Pip-Boy closed.
	//
	// Never call it while holding an inventory lock. The rebuild takes the
	// Pip-Boy lock, and the game takes that one first and the inventory second.
	// The other way round, 2 threads wait on each other forever.
	void Refresh(RE::ENUM_FORM_ID a_formType);
}
