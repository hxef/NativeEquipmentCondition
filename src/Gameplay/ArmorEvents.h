#pragma once

#include "Core/Plugin.h"

// Wear on the armor a blow lands on, the player's and everybody else's alike.
// The game raises a hit event for every blow that lands, with its damage and
// the limb it struck: a projectile names its own limb, a blow by hand is traced
// to the target's collision, a blast names none, and so do some bullets on a
// super mutant. The pieces covering that limb each lose some condition based on
// how hard the blow was before armor, and a blow that names no limb wears every
// piece on a limb. The wearer's race says which limb each slot covers, so a
// suit covers the torso, arms and legs, and a pair of goggles the head. A piece
// on no limb, a visor, a surgical mask or any piece on a super mutant, covers
// the whole body and wears from every blow, and the pieces on no limb split
// each blow between them. So a raider shot 5 times in the chest leaves a chest
// piece worse than one shot in the head, a super mutant's 3 pieces each take a
// third of every blow, and the player who takes hits pays for them in repairs.
// A piece worn to 0 stays on, see ArmorWear.h. An actor in power armor is
// skipped, since the game wears those pieces itself. An essential NPC's own
// outfit from the editor, a companion's for example, never wears, since some of
// it can never be handed to the player to repair. Whatever the player hands
// them wears as usual.
//
// After a piece wears, the engine is asked to add its wearer's resistances up
// again, so the piece protects less from the next blow on, see ArmorRating.h.
namespace ArmorEvents
{
	// Registers the hit sink once. The game keeps the hit event source through
	// every reload and full reset, and a second sink would wear every piece
	// twice.
	void Load();
}
