#include "Core/Text/Repair.h"

#include "Core/Text/Lines.h"
#include "Core/Text/Text.h"

#include <array>
#include <cstdint>
#include <format>

// The question both repair menus ask, the title of the bench's repair box and
// what the bench says around it, its spare box and the box of a spare's mods
// too, the [Repair] lines of the language files. A trader's own words are in
// Trader.cpp.
namespace Text
{
	std::string RepairQuestion(std::string_view a_name, std::string_view a_condition)
	{
		return Say(REPAIR_QUESTION, a_name, a_condition);
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

	std::string SparePerk(std::string_view a_perk, std::uint32_t a_rank)
	{
		return Say(SPARE_SCRAPPER, a_perk, a_rank);
	}

	std::string SpareQuestion(std::string_view a_name, std::string_view a_condition)
	{
		return Say(SPARE_QUESTION, a_name, a_condition);
	}

	std::string SparePerks(std::string_view a_pricing, std::uint32_t a_pricingRank,
		std::string_view a_scrapper, std::uint32_t a_scrapperRank)
	{
		return Say(SPARE_PERKS, a_pricing, a_pricingRank, a_scrapper, a_scrapperRank);
	}

	std::string SpareMark(bool a_equipped, bool a_favorite, bool a_legendary)
	{
		if (a_equipped) {
			return Pick(SPARE_EQUIPPED);
		}
		if (a_favorite) {
			return Pick(SPARE_FAVORITE);
		}
		return a_legendary ? Pick(SPARE_LEGENDARY) : std::string{};
	}

	std::string SpareCondition(std::string_view a_condition)
	{
		return Say(SPARE_CONDITION, a_condition);
	}

	std::string SpareRestores(std::string_view a_gain)
	{
		return Say(SPARE_RESTORES, a_gain);
	}

	std::string GainByType(std::int64_t a_gain, std::uint32_t a_type, bool a_armor, bool a_short)
	{
		// The card numbers its damage types from 1: physical, poison, fire,
		// energy, cryo and radiation, the same on OG and AE. Only physical
		// damage has a short word, the other damage words are short already.
		static constexpr std::array WEAPON{ DAMAGE_PHYSICAL, DAMAGE_POISON, DAMAGE_FIRE, DAMAGE_ENERGY, DAMAGE_CRYO, DAMAGE_RAD };
		static constexpr std::array WEAPON_SHORT{ DAMAGE_PHYSICAL_SHORT, DAMAGE_POISON, DAMAGE_FIRE, DAMAGE_ENERGY, DAMAGE_CRYO, DAMAGE_RAD };
		static constexpr std::array ARMOR{ RESIST_PHYSICAL, RESIST_POISON, RESIST_FIRE, RESIST_ENERGY, RESIST_CRYO, RESIST_RAD };
		static constexpr std::array ARMOR_SHORT{ RESIST_PHYSICAL_SHORT, RESIST_POISON_SHORT, RESIST_FIRE_SHORT, RESIST_ENERGY_SHORT,
			RESIST_CRYO_SHORT, RESIST_RAD_SHORT };
		if (a_type < 1 || a_type > WEAPON.size()) {
			return {};
		}
		const auto& words = a_armor ? (a_short ? ARMOR_SHORT : ARMOR) : (a_short ? WEAPON_SHORT : WEAPON);
		const auto* word = Pick(words[a_type - 1]);
		return Say(GAIN_BY_TYPE, a_gain, word);
	}

	std::string SpareSalvaged(std::span<const std::string> a_parts)
	{
		const auto parts = List(a_parts);
		return Say(SPARE_SALVAGED, parts);
	}

	std::string SpareKept(std::span<const std::string> a_mods, std::size_t a_more)
	{
		// With some left unnamed the list goes on past these names, so commas
		// join them all and no LIST_AND.
		if (a_more > 0) {
			const auto mods = Commas(a_mods, Out::kPage);
			return Say(SPARE_KEPT_MORE, mods, a_more);
		}
		const auto mods = List(a_mods);
		return Say(SPARE_KEPT, mods);
	}

	std::string SpareModQuestion(std::string_view a_name, std::string_view a_from)
	{
		return Say(SPARE_MOD_QUESTION, a_name, a_from);
	}

	std::string SpareModFull(std::string_view a_name)
	{
		return Say(SPARE_MOD_FULL, a_name);
	}

	std::string SpareModKeepQuestion(std::string_view a_name, std::string_view a_to)
	{
		return Say(SPARE_MOD_KEEP_QUESTION, a_name, a_to);
	}

	std::string SpareUseUpRest()
	{
		return Pick(SPARE_USE_UP_REST);
	}

	std::string SpareKeepRest()
	{
		return Pick(SPARE_KEEP_REST);
	}

	std::string SpareModValue(std::uint32_t a_caps)
	{
		const auto caps = RepairCaps(a_caps);
		return Say(SPARE_MOD_VALUE, caps);
	}

	std::string SpareModGain(std::string_view a_number)
	{
		return Say(SPARE_MOD_GAIN, a_number);
	}

	std::string BenchCannotRepair(bool a_trader)
	{
		const auto* cannot = Pick(BENCH_CANNOT_REPAIR);
		return a_trader ? Say(SENTENCES, cannot, Pick(REPAIR_LOCKED_TRADER)) : std::string{ cannot };
	}

	std::string RepairLocked(std::string_view a_perk, std::uint32_t a_rank, std::string_view a_condition, bool a_spare, bool a_trader)
	{
		const auto needs = Say(REPAIR_LOCKED, a_perk, a_rank, a_condition);
		if (!a_spare && !a_trader) {
			return needs;
		}
		const auto* way = a_spare && a_trader ? Pick(REPAIR_LOCKED_BOTH) : Pick(a_spare ? REPAIR_LOCKED_SPARE : REPAIR_LOCKED_TRADER);
		return Say(SENTENCES, needs, way);
	}

	std::string CannotModify()
	{
		return Pick(CANNOT_MODIFY);
	}
}
