#include "Core/Text/Lines.h"

#include "Core/Parts.h"

// The names of the Worn loot parts, from Worn loot to Quest rewards arrive new,
// see Core/Parts.h.
namespace Text
{
	namespace
	{
		constexpr PartRow PARTS[]{
			{ Part::kSpawn, PART_SPAWN },
			{ Part::kConsoleNew, PART_CONSOLE_NEW },
			{ Part::kWorldLoot, PART_WORLD_LOOT },
			{ Part::kModlessLoot, PART_MODLESS_LOOT },
			{ Part::kGifts, PART_GIFTS },
		};

		static_assert(PartsInOrder(PARTS, Part::kSpawn, Part::kGifts));
	}

	std::span<const PartRow> LootParts()
	{
		return PARTS;
	}
}
