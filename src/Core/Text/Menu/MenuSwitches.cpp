#include "Core/Text/Lines.h"

#include "Core/Settings/Settings.h"

// The help lines of the switches on the MCM page, the names and help of the
// All NEC features, plain apparel, workbench repairs, spare repair, lock
// spares, repair columns, repair gains, trader sign, card wear rate, power
// armor CND, CND column, fridge icon and bug report logs switches, and of
// the Pip-Boy repairs, unscrappable spares, keep spare mods, spare mod rows,
// repair box, repair lock, 4 CND and CND decimals steppers, see MenuLine in
// Text.h.
namespace Text
{
	namespace
	{
		constexpr MenuRow ROWS[]{
			{ &Settings::bNecOn, NEC_ON_NAME, NEC_ON_HELP },
			{ &Settings::bPlainApparelNeverWears, PLAIN_APPAREL_NAME, PLAIN_APPAREL_HELP },
			{ &Settings::bSpawnCondition, {}, SPAWN_CONDITION_HELP },
			{ &Settings::bWorkbenchRepair, WORKBENCH_REPAIR_NAME, WORKBENCH_REPAIR_HELP },
			{ &Settings::bVendorRepair, {}, VENDOR_REPAIR_HELP },
			{ &Settings::bTraderSign, TRADER_SIGN_NAME, TRADER_SIGN_HELP },
			{ &Settings::bSpareRepair, SPARE_REPAIR_NAME, SPARE_REPAIR_HELP },
			{ &Settings::iPipboyRepair, PIPBOY_REPAIR_NAME, PIPBOY_REPAIR_HELP },
			{ &Settings::iUnscrappableSpares, UNSCRAPPABLE_SPARES_NAME, UNSCRAPPABLE_SPARES_HELP },
			{ &Settings::iKeepSpareMods, KEEP_SPARE_MODS_NAME, KEEP_SPARE_MODS_HELP },
			{ &Settings::iSpareModRows, SPARE_MOD_ROWS_NAME, SPARE_MOD_ROWS_HELP },
			{ &Settings::iRepairBoxRow, REPAIR_BOX_ROW_NAME, REPAIR_BOX_ROW_HELP },
			{ &Settings::iRepairLock, REPAIR_LOCK_NAME, REPAIR_LOCK_HELP },
			{ &Settings::bLockSpares, LOCK_SPARES_NAME, LOCK_SPARES_HELP },
			{ &Settings::bRepairBoxColumns, REPAIR_COLUMNS_NAME, REPAIR_COLUMNS_HELP },
			{ &Settings::bRepairBoxGains, REPAIR_GAINS_NAME, REPAIR_GAINS_HELP },
			{ &Settings::bLoadingTips, {}, LOADING_TIPS_HELP },
			{ &Settings::iWeaponCnd, HUD_CONDITION_NAME, HUD_CONDITION_HELP },
			{ &Settings::iPowerArmorCnd, POWER_ARMOR_BAR_NAME, POWER_ARMOR_BAR_HELP },
			{ &Settings::iQuickContainerCnd, QUICK_CONTAINER_NAME, QUICK_CONTAINER_HELP },
			{ &Settings::iItemCardCnd, CARD_CND_NAME, CARD_CND_HELP },
			{ &Settings::bCardWearRate, CARD_WEAR_RATE_NAME, CARD_WEAR_RATE_HELP },
			{ &Settings::bPowerArmorCardCnd, POWER_ARMOR_CND_NAME, POWER_ARMOR_CND_HELP },
			{ &Settings::iCndDecimals, CND_DECIMALS_NAME, CND_DECIMALS_HELP },
			{ &Settings::bCndColumn, CND_COLUMN_NAME, CND_COLUMN_HELP },
			{ &Settings::bConfirmScroll, {}, CONFIRM_SCROLL_HELP },
			{ &Settings::bInspectPrice, {}, INSPECT_PRICE_HELP },
			{ &Settings::bFoodSpoils, {}, FOOD_SPOILS_HELP },
			{ &Settings::bFridgeIcon, FRIDGE_ICON_NAME, FRIDGE_ICON_HELP },
			{ &Settings::bTraceLogs, TRACE_LOGS_NAME, TRACE_LOGS_HELP },
		};
	}

	std::span<const MenuRow> MenuSwitches()
	{
		return ROWS;
	}
}
