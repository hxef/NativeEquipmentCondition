#include "Core/Text/Lines.h"

#include "Core/Parts.h"

// The names of the parts of the UI rows, see Core/Parts.h.
namespace Text
{
	namespace
	{
		constexpr PartRow PARTS[]{
			{ Part::kCardCnd, PART_CARD_CND },
			{ Part::kCardRate, PART_CARD_RATE },
			{ Part::kCardAccuracy, PART_CARD_ACCURACY },
			{ Part::kCardRange, PART_CARD_RANGE },
			{ Part::kSort, PART_SORT },
			{ Part::kHudBar, PART_HUD_BAR },
			{ Part::kQuick, PART_QUICK },
			{ Part::kBench, PART_BENCH },
			{ Part::kBenchLists, PART_BENCH_LISTS },
			{ Part::kScroll, PART_SCROLL },
			{ Part::kTraderRepairs, PART_TRADER_REPAIRS },
			{ Part::kStock, PART_STOCK },
			{ Part::kInspect, PART_INSPECT },
			{ Part::kSrm, PART_SRM },
			{ Part::kTips, PART_TIPS },
		};

		static_assert(PartsInOrder(PARTS, Part::kCardCnd, Part::kTips));
	}

	std::span<const PartRow> UiParts()
	{
		return PARTS;
	}
}
