#include "Core/Text/Lines.h"

#include "Core/Parts.h"

// The names of the parts of the Condition and Gameplay rows up to Single shot
// gun jams, see Core/Parts.h. The Worn loot parts are in PartsLoot.cpp.
namespace Text
{
	namespace
	{
		constexpr PartRow PARTS[]{
			{ Part::kPerks, PART_PERKS },
			{ Part::kGunWear, PART_GUN_WEAR },
			{ Part::kDamage, PART_DAMAGE },
			{ Part::kCardDamage, PART_CARD_DAMAGE },
			{ Part::kArmor, PART_ARMOR },
			{ Part::kCardArmor, PART_CARD_ARMOR },
			{ Part::kBroken, PART_BROKEN },
			{ Part::kPrices, PART_PRICES },
			{ Part::kFireRate, PART_FIRE_RATE },
			{ Part::kFireSound, PART_FIRE_SOUND },
			{ Part::kCritMeter, PART_CRIT_METER },
			{ Part::kNpcCrits, PART_NPC_CRITS },
			{ Part::kAccuracy, PART_ACCURACY },
			{ Part::kCrosshair, PART_CROSSHAIR },
			{ Part::kVatsAccuracy, PART_VATS_ACCURACY },
			{ Part::kRange, PART_RANGE },
			{ Part::kJam, PART_JAM },
			{ Part::kReloadJam, PART_RELOAD_JAM },
		};

		static_assert(PartsInOrder(PARTS, Part::kPerks, Part::kReloadJam));
	}

	std::span<const PartRow> PlayParts()
	{
		return PARTS;
	}
}
