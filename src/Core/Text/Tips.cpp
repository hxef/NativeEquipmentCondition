#include "Core/Text/Text.h"

#include "Core/Text/Lines.h"

// The tips the loading screen shows now and then, see LoadingTips.h, the
// [Tips] lines of the language files.
namespace Text
{
	std::string WearTip()
	{
		return Pick(TIP_WEAR);
	}

	std::string BenchTip()
	{
		return Pick(TIP_BENCH);
	}

	std::string TraderTip()
	{
		return Pick(TIP_TRADER);
	}

	std::string BrokenTip()
	{
		return Pick(TIP_BROKEN);
	}

	std::string JamTip()
	{
		return Pick(TIP_JAM);
	}

	std::string LootTip()
	{
		return Pick(TIP_LOOT);
	}
}
