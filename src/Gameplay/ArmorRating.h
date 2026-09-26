#pragma once

#include "Core/Plugin.h"

// A worn piece of armor protects less. The game adds armor up in 2 places. The
// rating is the physical resistance, added up over the equipped pieces every
// time DamageResist is read, by a visitor the actor runs, with a tempering
// bonus from the piece's item health that Skyrim left behind and Fallout 4's
// settings keep at 0. The damage type values, energy, radiation and the rest,
// are added up once per change of clothes and written into the actor values.
// Install hooks both sums and scales each piece by its condition, a straight
// line from fArmorFloor at 0 to 1 at full, the same line as the damage. The
// item cards and the Pip-Boy's paper doll add a piece up in a third place, and
// Install scales that one too, so the menus show the worn numbers.
//
// The rating is patched at the calls to the visitor: the visitor adds what a
// piece is worth to its total, and the hook scales what it added, which keeps
// the perk entry point the visitor runs and reads the equipped copy's health
// where the visitor itself reads the first stack's. After a piece wears or is
// repaired, Refresh asks the engine to add both sums up again, as a change of
// clothes does.
//
// Everybody's armor. A raider's worn chest piece protects the raider less,
// and once looted and put on protects the player less, until it is repaired.
namespace ArmorRating
{
	// Patches the 2 sums and the menus' one.
	void Install();

	// The share of its protection a piece keeps at a_health, see
	// Condition::Share.
	float Share(float a_health);

	// Asks the engine to add a_actor's resistances up again, after a piece
	// wore or was repaired. Equipping does the same on its own.
	void Refresh(RE::Actor& a_actor);
}
