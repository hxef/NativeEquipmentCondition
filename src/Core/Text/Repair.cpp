#include "Core/Text/Text.h"

#include "Core/Text/Lines.h"

// The question both repair menus ask, the title of the bench's repair box and
// what the bench says around it, its spare box too, the [Repair] lines of the
// language files. A trader's own words are in Trader.cpp.
namespace Text
{
	std::string RepairQuestion(std::string_view a_name, std::uint32_t a_percent)
	{
		return Say(REPAIR_QUESTION, a_name, a_percent);
	}

	std::string RepairBoxTitle(std::string_view a_name, std::uint32_t a_percent)
	{
		return Say(REPAIR_BOX_TITLE, a_name, a_percent);
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

	std::string SpareButton()
	{
		return Pick(SPARE_BUTTON);
	}

	std::string SpareScrapper(std::string_view a_perk, std::uint32_t a_rank)
	{
		return Say(SPARE_SCRAPPER, a_perk, a_rank);
	}

	std::string SpareQuestion(std::string_view a_name, std::uint32_t a_percent)
	{
		return Say(SPARE_QUESTION, a_name, a_percent);
	}

	std::string SpareName(std::string_view a_name, bool a_equipped, bool a_favorite, bool a_legendary)
	{
		// Each mark goes in front of the name, legendary first, so the row reads
		// equipped, favorite, legendary.
		std::string out{ a_name };
		if (a_legendary) {
			out = std::format("{:s} {:s}", Pick(SPARE_LEGENDARY), out);
		}
		if (a_favorite) {
			out = std::format("{:s} {:s}", Pick(SPARE_FAVORITE), out);
		}
		if (a_equipped) {
			out = std::format("{:s} {:s}", Pick(SPARE_EQUIPPED), out);
		}
		return out;
	}

	std::string SpareRow(std::string_view a_name, std::uint32_t a_percent, std::uint32_t a_gain)
	{
		return Say(SPARE_ROW, a_name, a_percent, a_gain);
	}

	std::string SpareSalvaged(std::span<const std::string> a_parts)
	{
		const auto parts = List(a_parts);
		return Say(SPARE_SALVAGED, parts);
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
