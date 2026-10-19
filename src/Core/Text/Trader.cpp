#include "Core/Text/Text.h"

#include "Core/Text/Lines.h"

// What a trader says about a repair, the [Trader] lines of the language files.
namespace Text
{
	namespace
	{
		// The line of the 3 for a_trade.
		[[nodiscard]] Key Of(Trade a_trade, Key a_weapons, Key a_armor, Key a_clothing)
		{
			switch (a_trade) {
			case Trade::kArmor:
				return a_armor;
			case Trade::kClothing:
				return a_clothing;
			default:
				return a_weapons;
			}
		}
	}

	std::string RepairPrice(std::uint32_t a_level, std::uint32_t a_caps)
	{
		return Say(REPAIR_PRICE, a_level, a_caps);
	}

	std::string RepairPaid(std::uint32_t a_level, std::uint32_t a_caps)
	{
		return Say(REPAIR_PAID, a_level, a_caps);
	}

	std::string RepairUpTo(std::uint32_t a_ceiling, Trade a_trade)
	{
		if (a_ceiling >= FULL) {
			return Pick(Of(a_trade, REPAIR_UP_TO_FULL_WEAPONS, REPAIR_UP_TO_FULL_ARMOR, REPAIR_UP_TO_FULL_CLOTHING));
		}
		return Say(Of(a_trade, REPAIR_UP_TO_WEAPONS, REPAIR_UP_TO_ARMOR, REPAIR_UP_TO_CLOTHING), a_ceiling);
	}

	std::string RepairCeiling(std::uint32_t a_ceiling, Trade a_trade)
	{
		return Say(Of(a_trade, REPAIR_CEILING_WEAPONS, REPAIR_CEILING_ARMOR, REPAIR_CEILING_CLOTHING), a_ceiling);
	}

	std::string RepairUnaffordable()
	{
		return Pick(REPAIR_UNAFFORDABLE);
	}

	std::string RepairTradePending()
	{
		return Pick(REPAIR_TRADE_PENDING);
	}
}
