#pragma once

#include "Core/Plugin.h"

// The Pip-Boy keeps an item's card until it lists the item again, so an item
// whose condition changed could keep printing what it had. The game's own
// equip handler rebuilds a whole category, and this calls that rebuild.
//
// Wear needs none. Equipped::WriteStack tells the game the player's
// inventory changed, and the Pip-Boy lists that item again by itself. A
// rebuild on every shot or hit would redo the card of every item of that
// kind the player carries. A repair, the console and the MCM still ask for
// one, once per change, where it costs little.
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
