#include "Core/Text/Lines.h"

#include "Core/Settings.h"

// The names and help lines of the Balance numbers on the MCM page after the
// wear speeds, which are in MenuWear.cpp, see MenuLine in Text.h.
namespace Text
{
	namespace
	{
		constexpr MenuRow ROWS[]{
			{ &Settings::fDamageFloor, DAMAGE_FLOOR_NAME, DAMAGE_FLOOR_HELP },
			{ &Settings::fWeaponPenaltyStart, WEAPON_START_NAME, WEAPON_START_HELP },
			{ &Settings::fArmorFloor, ARMOR_FLOOR_NAME, ARMOR_FLOOR_HELP },
			{ &Settings::fArmorPenaltyStart, ARMOR_START_NAME, ARMOR_START_HELP },
			{ &Settings::fValueExponent, VALUE_EXPONENT_NAME, VALUE_EXPONENT_HELP },
			{ &Settings::fFireRateFloor, FIRE_RATE_FLOOR_NAME, FIRE_RATE_FLOOR_HELP },
			{ &Settings::fCritMeterFloor, CRIT_METER_FLOOR_NAME, CRIT_METER_FLOOR_HELP },
			{ &Settings::fAccuracyFloor, ACCURACY_FLOOR_NAME, ACCURACY_FLOOR_HELP },
			{ &Settings::fRangeFloor, RANGE_FLOOR_NAME, RANGE_FLOOR_HELP },
			{ &Settings::fBenchCostMult, BENCH_COST_NAME, BENCH_COST_HELP },
			{ &Settings::iFreeMendAbove, FREE_MEND_NAME, FREE_MEND_HELP },
			{ &Settings::fTraderPriceMult, TRADER_PRICE_NAME, TRADER_PRICE_HELP },
		};
	}

	std::span<const MenuRow> MenuNumbers()
	{
		return ROWS;
	}
}
