#pragma once

#include "Core/Plugin.h"
#include "Gameplay/SpawnCondition/SpawnCondition.h"

#include <optional>

// The roll itself, either side of an item's middle or in a trader's band.
// Private to this folder.
namespace SpawnCondition
{
	// How far either side of its middle an item rolls. Fallout 3 had no
	// formula, its lists held copies of one gun at authored values, common loot
	// around 15 to 60%. The middle is Provenance's answer for this item, so
	// this band is the variety inside one raider camp: 2 raiders differ by a
	// little and from a Courser by a lot.
	inline constexpr float SPREAD = 0.12F;

	// How often an item ignores its band and rolls across the whole scale. 1
	// item in 100 does, a Courser's beaten rifle or a raider's pristine one.
	// For loot the rare roll nearly always lands outside its band, and the
	// further the band is from the middle, the bigger the surprise. Kept rare,
	// since at about 1 in 20 it would stop being a surprise. A trader's own
	// stock never rolls lower than its StockBand's worst.
	inline constexpr float UPSET_CHANCE = 0.01F;

	// The worst condition an item arrives at. A weapon at 0 cannot be equipped,
	// see MIN_HEALTH in Condition.h, so an NPC given one would spawn unarmed. 0
	// is reached by use, never by the roll.
	inline constexpr float WORST_SPAWN = 0.01F;

	// The limits the ordinary roll stays between, in order, since the 4 numbers
	// they come from live in different files and a clamp with its limits
	// crossed is undefined behaviour. Worked out in one place, because Install
	// prints them and RollHealth rolls inside them.
	struct Ends
	{
		float floor{ 0.0F };
		float ceiling{ 0.0F };
	};

	Ends OrdinaryEnds();

	// What a roll aims at: the middle of its band, or for a trader's own stock
	// the trader's band, which takes its place.
	struct Aim
	{
		float                    centre{ 0.0F };
		std::optional<StockBand> stock;
	};

	// What one roll came out as, and whether it was one of the rare ones.
	struct Rolled
	{
		float health{ 0.0F };
		bool  upset{ false };
	};

	Rolled RollHealth(const Aim& a_aim);
}
