#include "Core/Text/Lines.h"

#include "Core/Settings.h"

// The help lines of the switches on the MCM page, and the names and help of
// the plain apparel, 2 CND bar, fridge icon and bug report logs switches, see
// MenuLine in Text.h.
namespace Text
{
	namespace
	{
		constexpr MenuRow ROWS[]{
			{ &Settings::bJam, {}, JAM_HELP },
			{ &Settings::bPlainApparelNeverWears, PLAIN_APPAREL_NAME, PLAIN_APPAREL_HELP },
			{ &Settings::bSpawnCondition, {}, SPAWN_CONDITION_HELP },
			{ &Settings::bVendorRepair, {}, VENDOR_REPAIR_HELP },
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
