#include "Core/Text/Text.h"

#include "Core/Text/Lines.h"

// The question both repair menus ask and what the bench says around it, the
// [Repair] lines of the language files. A trader's own words are in Trader.cpp.
namespace Text
{
	std::string RepairQuestion(std::string_view a_name, std::uint32_t a_percent)
	{
		return Say(REPAIR_QUESTION, a_name, a_percent);
	}

	std::string RepairDiscount(std::string_view a_perk, std::uint32_t a_rank, std::uint32_t a_percent)
	{
		return Say(REPAIR_DISCOUNT, a_perk, a_rank, a_percent);
	}

	std::string TooDamaged(std::uint32_t a_floor)
	{
		return a_floor >= FULL ? Pick(TOO_DAMAGED_FULL) : Say(TOO_DAMAGED, a_floor);
	}

	std::string MendButton()
	{
		return Pick(MEND_BUTTON);
	}

	std::string Mended(bool a_armor)
	{
		return Pick(a_armor ? MENDED_ARMOR : MENDED_WEAPON);
	}

	std::string BenchCannotRepair()
	{
		return Pick(BENCH_CANNOT_REPAIR);
	}

	std::string CannotModify()
	{
		return Pick(CANNOT_MODIFY);
	}
}
