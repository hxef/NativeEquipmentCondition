#include "Core/Text/Lines.h"

#include "Core/Settings.h"

// The help lines of the switches on the MCM page, the names and help of the
// plain apparel, 2 CND bar, spare repair, repair columns, repair gains, trader
// sign, fridge icon and bug report logs switches, and of the unscrappable
// spares and repair box steppers, see MenuLine in Text.h.
namespace Text
{
	namespace
	{
		constexpr MenuRow ROWS[]{
			{ &Settings::bPlainApparelNeverWears, PLAIN_APPAREL_NAME, PLAIN_APPAREL_HELP },
			{ &Settings::bSpawnCondition, {}, SPAWN_CONDITION_HELP },
			{ &Settings::bVendorRepair, {}, VENDOR_REPAIR_HELP },
			{ &Settings::bTraderSign, TRADER_SIGN_NAME, TRADER_SIGN_HELP },
			{ &Settings::bSpareRepair, SPARE_REPAIR_NAME, SPARE_REPAIR_HELP },
			{ &Settings::iUnscrappableSpares, UNSCRAPPABLE_SPARES_NAME, UNSCRAPPABLE_SPARES_HELP },
			{ &Settings::iRepairBoxRow, REPAIR_BOX_ROW_NAME, REPAIR_BOX_ROW_HELP },
			{ &Settings::bRepairBoxColumns, REPAIR_COLUMNS_NAME, REPAIR_COLUMNS_HELP },
			{ &Settings::bRepairBoxGains, REPAIR_GAINS_NAME, REPAIR_GAINS_HELP },
			{ &Settings::bLoadingTips, {}, LOADING_TIPS_HELP },
			{ &Settings::bHudCondition, HUD_CONDITION_NAME, HUD_CONDITION_HELP },
			{ &Settings::bPowerArmorBar, POWER_ARMOR_BAR_NAME, POWER_ARMOR_BAR_HELP },
			{ &Settings::bQuickContainer, {}, QUICK_CONTAINER_HELP },
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
