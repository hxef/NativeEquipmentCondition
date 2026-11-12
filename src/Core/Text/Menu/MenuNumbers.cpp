#include "Core/Text/Lines.h"

#include "Core/Settings.h"

// The names and help lines of the numbers on the MCM page other than the
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
			{ &Settings::iLootShift, LOOT_SHIFT_NAME, LOOT_SHIFT_HELP },
			{ &Settings::fValueExponent, VALUE_EXPONENT_NAME, VALUE_EXPONENT_HELP },
			{ &Settings::fFireRateFloor, FIRE_RATE_FLOOR_NAME, FIRE_RATE_FLOOR_HELP },
			{ &Settings::fCritMeterFloor, CRIT_METER_FLOOR_NAME, CRIT_METER_FLOOR_HELP },
			{ &Settings::fAccuracyFloor, ACCURACY_FLOOR_NAME, ACCURACY_FLOOR_HELP },
			{ &Settings::fRangeFloor, RANGE_FLOOR_NAME, RANGE_FLOOR_HELP },
			{ &Settings::fJamChance, JAM_CHANCE_NAME, JAM_CHANCE_HELP },
			{ &Settings::fJamStart, JAM_START_NAME, JAM_START_HELP },
			{ &Settings::fBenchCostMult, BENCH_COST_NAME, BENCH_COST_HELP },
			{ &Settings::fSpareWorthMult, SPARE_WORTH_NAME, SPARE_WORTH_HELP },
			{ &Settings::iFreeMendAbove, FREE_MEND_NAME, FREE_MEND_HELP },
			{ &Settings::fModLockStart, MOD_LOCK_NAME, MOD_LOCK_HELP },
			{ &Settings::fTraderPriceMult, TRADER_PRICE_NAME, TRADER_PRICE_HELP },
			{ &Settings::iTraderLeeway, TRADER_LEEWAY_NAME, TRADER_LEEWAY_HELP },
			{ &Settings::iTraderFullStock, TRADER_FULL_STOCK_NAME, TRADER_FULL_STOCK_HELP },
			{ &Settings::fRawFoodDays, RAW_FOOD_DAYS_NAME, RAW_FOOD_DAYS_HELP },
			{ &Settings::fCookedFoodDays, COOKED_FOOD_DAYS_NAME, COOKED_FOOD_DAYS_HELP },
			{ &Settings::fFridgeSpoilSpeed, FRIDGE_SPEED_NAME, FRIDGE_SPEED_HELP },
			{ &Settings::iSpoiledPerFertilizer, SPOILED_PER_FERTILIZER_NAME, SPOILED_PER_FERTILIZER_HELP },
			{ &Settings::iSpoiledHealing, SPOILED_HEALING_NAME, SPOILED_HEALING_HELP },
			{ &Settings::iSpoiledHunger, SPOILED_HUNGER_NAME, SPOILED_HUNGER_HELP },
			{ &Settings::fSpoiledRadsMult, SPOILED_RADS_NAME, SPOILED_RADS_HELP },
			{ &Settings::iSpoiledSicknessMult, SPOILED_SICKNESS_NAME, SPOILED_SICKNESS_HELP },
		};
	}

	std::span<const MenuRow> MenuNumbers()
	{
		return ROWS;
	}
}
