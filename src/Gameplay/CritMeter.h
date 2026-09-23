#pragma once

#include "Core/Plugin.h"

namespace CritMeter
{
	// A worn weapon fills the VATS critical meter slower, and an NPC's worn
	// weapon lands fewer critical hits.
	//
	// Every VATS shot adds some charge to the meter, worked out by
	// CombatFormulas::CalcVATSCriticalCharge from Luck, the weapon's critical
	// charge bonus and the perks. Install hooks the one call VATS makes to it
	// and scales the result by the condition of the weapon in hand. Unarmed
	// shots name the race's bare hands weapon, which has no condition, so they
	// fill the meter as usual.
	//
	// NPCs have no meter. Every blow rolls for a critical in
	// HitData::RollCritical at a chance from the attacker's CritChance actor
	// value and the perks, and Install scales an NPC's chance by the weapon's
	// condition on the same curve. The player's roll is left alone, since the
	// player's criticals already follow the meter.
	void Install();
}
