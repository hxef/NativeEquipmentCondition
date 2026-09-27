#pragma once

#include "Core/Plugin.h"

// Which armor takes part, how fast a piece wears, and wearing down a piece
// someone has on. Every hit that lands costs the pieces it reaches some
// condition, based on how hard the hit was compared with the ordinary weapon of
// the load order and on what the piece is built of, see Materials.h. Rate.h has
// the rate. ArmorEvents decides which hits wear which pieces.
//
// Power armor pieces never take part. The game already wears them with its own
// rules, breaks them at 0 and repairs them at the station, so wearing them here
// too would count every hit twice.
namespace ArmorWear
{
	// Says in the log how armor wears. Keeps no forms, so there is nothing to
	// unload.
	void Load();

	// Whether this armor takes part, as a reason: nullptr for one that does,
	// otherwise what ruled it out. Every piece the player can carry does, from
	// a chest piece to a hat, as every piece of clothing wore in the older
	// games, unless it gives nothing: no protection, no effect and no slot for
	// a mod, like a wedding ring or Dogmeat's bandana. A piece the player can
	// never carry, a super mutant's own chest piece or a disguise an NPC wears,
	// takes part when it has a name and protects against something, so a
	// creature's armor wears the way a raider's does. What is left, a skin, a
	// shell or a set of rags, stays out. Power armor pieces stay out too, see
	// above.
	const char* WhyNoCondition(const RE::TESObjectARMO& a_armor);

	// Whether a piece is clothing rather than armor, to a trader and for the
	// recipe a piece with none of its own borrows, see Materials.h. The game
	// keeps no flag for it: the clothing and armor keywords it ships sit on
	// no record. What it does keep is the layering. Every outfit, suit, vault
	// suit and under armor sits on the body slot, 33, and no arm, leg or chest
	// piece and no helmet does. So clothing is a piece on the body slot or one
	// that protects nothing, like a hat or a pair of glasses, and armor is
	// the rest, a chest piece, a helmet, a gas mask. A hazmat suit is
	// clothing under this and a hard hat is armor.
	bool IsClothing(const RE::TESObjectARMO& a_armor);

	// Wears down the copy of a piece a_owner has on, and that copy alone, see
	// Equipped::WriteEquipped. a_source names the hit on the trace line, and
	// a_theirs sends the line to the NPC log. A piece worn to 0 stays on and
	// the player is told. The game refuses to put a broken piece back on, so
	// once the player takes it off it has to be repaired before it can be worn
	// again, see BrokenEquip.h. Returns true when the health changed. Call it
	// without holding the inventory lock, see ItemCards.h.
	bool Wear(RE::Actor& a_owner, RE::TESObjectARMO& a_armor, float a_damage, const char* a_source, bool a_theirs);
}
