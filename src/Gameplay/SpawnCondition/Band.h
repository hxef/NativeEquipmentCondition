#pragma once

#include "Core/Plugin.h"
#include "Gameplay/SpawnCondition/SpawnCondition.h"

#include <cstdint>
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

	// The worst condition an item arrives at. A weapon at 0 cannot be equipped,
	// see MIN_HEALTH in Condition.h, so an NPC given one would spawn unarmed. 0
	// is reached by use, never by the roll.
	inline constexpr float WORST_SPAWN = 0.01F;

	// The limits the ordinary roll stays between, in order, since the 4 numbers
	// they come from live in different files and a clamp with its limits
	// crossed is undefined behaviour.
	struct Ends
	{
		float floor{ 0.0F };
		float ceiling{ 0.0F };
	};

	// The Loot condition shift slider, -100 to 100. Read at each roll and
	// never kept, so a change on the MCM page counts from the next roll.
	std::int32_t LootShift();

	// 1 item in this many ignores its band and rolls across the whole scale, a
	// Courser's beaten rifle or a raider's pristine one. The Surprise loot 1 in
	// slider, 100 by default and 0 to 1000, read at each roll and never kept,
	// so a change on the MCM page counts from the next item. 0 means no item
	// does. For loot the rare roll nearly always lands outside its band, and
	// the further the band is from the middle, the bigger the surprise. A
	// trader's stock rolls it too, and never lower than its StockBand's worst.
	std::int32_t LootSurpriseOdds();

	// Where the ordinary roll lands for an item whose middle is a_centre, with
	// the slider at a_shift: the middle the slider moved it to, the ends and
	// the band inside them. Worked out in one place, because Install prints
	// it and RollHealth rolls inside it.
	struct Band
	{
		float centre{ 0.0F };
		Ends  ends;
		float low{ 0.0F };
		float high{ 0.0F };
	};

	Band OrdinaryBand(float a_centre, std::int32_t a_shift);

	// What a roll aims at: the middle of its band, or for a trader's own stock
	// the trader's band, which takes its place.
	struct Aim
	{
		float                    centre{ 0.0F };
		std::optional<StockBand> stock;
	};

	// What one roll came out as, and whether it was one of the rare ones. An
	// ordinary roll the slider moved keeps the slider and the middle it aimed
	// at, for the trace log.
	struct Rolled
	{
		float        health{ 0.0F };
		bool         upset{ false };
		std::int32_t shift{ 0 };
		float        centre{ 0.0F };
	};

	Rolled RollHealth(const Aim& a_aim);
}
