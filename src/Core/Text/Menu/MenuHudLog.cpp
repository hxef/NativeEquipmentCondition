#include "Core/Text/Lines.h"

#include "Core/Settings.h"

// The names and help lines of the HUD numbers and the log level on the MCM
// page, see MenuLine in Text.h, and the HUD bar sliders' Default sentence.
namespace Text
{
	namespace
	{
		// sLogLevel is the one setting that is no Live one, so its row names
		// the key itself.
		constexpr Settings::Named LOG_LEVEL{ "Log", "sLogLevel" };

		constexpr MenuRow ROWS[]{
			{ &Settings::fHudBarX, HUD_BAR_X_NAME, HUD_BAR_X_HELP },
			{ &Settings::fHudBarY, HUD_BAR_Y_NAME, HUD_BAR_Y_HELP },
			{ &Settings::fPowerArmorBarX, POWER_ARMOR_BAR_X_NAME, POWER_ARMOR_BAR_X_HELP },
			{ &Settings::fPowerArmorBarY, POWER_ARMOR_BAR_Y_NAME, POWER_ARMOR_BAR_Y_HELP },
			{ &LOG_LEVEL, LOG_LEVEL_NAME, LOG_LEVEL_HELP },
		};
	}

	std::span<const MenuRow> MenuHudLog()
	{
		return ROWS;
	}

	std::string MenuHudBarDefault(std::string_view a_help, std::int32_t a_number)
	{
		return Sentences(a_help, Say(HUD_BAR_DEFAULT, a_number), Out::kPage);
	}
}
