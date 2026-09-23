#pragma once

#include "Core/Plugin.h"

// How far either side of its middle a weapon rolls. Private to this folder.
namespace SpawnCondition
{
	// How far either side of its middle a weapon rolls. Fallout 3 had no
	// formula, its lists held copies of one gun at authored values, common loot
	// around 15 to 60%. The middle is Provenance's answer for this weapon, so
	// this band is the variety inside one raider camp: 2 raiders differ by a
	// little and from a Courser by a lot.
	inline constexpr float SPREAD = 0.12F;

	// How often a weapon ignores its band and rolls across the whole scale. 1
	// gun in 100 does, a Courser's beaten rifle or a raider's pristine one. The
	// rare roll nearly always lands outside its band, and the further the band
	// is from the middle, the bigger the surprise. Kept rare, since at about 1
	// in 20 it would stop being a surprise.
	inline constexpr float UPSET_CHANCE = 0.01F;

	// The worst condition a weapon arrives at. A weapon at 0 cannot be
	// equipped, see MIN_HEALTH in Condition.h, so an NPC given one would spawn
	// unarmed. 0 is reached by use, never by the roll.
	inline constexpr float WORST_SPAWN = 0.01F;

	// The limits the ordinary roll stays between, in order, since the 4 numbers
	// they come from live in different files and a clamp with its limits
	// crossed is undefined behaviour. Worked out once, because Install prints
	// them and RollHealth rolls inside them.
	struct Ends
	{
		float floor{ 0.0F };
		float ceiling{ 0.0F };
	};

	Ends OrdinaryEnds();

	// What one roll came out as, and whether it was one of the rare ones.
	struct Rolled
	{
		float health{ 0.0F };
		bool  upset{ false };
	};

	Rolled RollHealth(float a_centre);
}
