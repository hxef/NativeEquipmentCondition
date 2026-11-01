#include "Core/Text/Lines.h"

#include "Core/Settings.h"

// The names and help lines of the HUD settings and the log level on the MCM
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
			{ &Settings::fHudBarLength, HUD_BAR_LENGTH_NAME, HUD_BAR_LENGTH_HELP },
			{ &Settings::fHudBarThickness, HUD_BAR_THICKNESS_NAME, HUD_BAR_THICKNESS_HELP },
			{ &Settings::bHudBarHidesWithCounter, HUD_BAR_HIDES_NAME, HUD_BAR_HIDES_HELP },
			{ &Settings::bHudBarShowsWord, HUD_BAR_WORD_NAME, HUD_BAR_WORD_HELP },
			{ &Settings::fPowerArmorBarX, POWER_ARMOR_BAR_X_NAME, POWER_ARMOR_BAR_X_HELP },
			{ &Settings::fPowerArmorBarY, POWER_ARMOR_BAR_Y_NAME, POWER_ARMOR_BAR_Y_HELP },
			{ &Settings::fPowerArmorBarLength, POWER_ARMOR_BAR_LENGTH_NAME, POWER_ARMOR_BAR_LENGTH_HELP },
			{ &Settings::fPowerArmorBarThickness, POWER_ARMOR_BAR_THICKNESS_NAME, POWER_ARMOR_BAR_THICKNESS_HELP },
			{ &Settings::bPowerArmorBarShowsWord, POWER_ARMOR_BAR_WORD_NAME, POWER_ARMOR_BAR_WORD_HELP },
			{ &Settings::bPickupBar, PICKUP_BAR_NAME, PICKUP_BAR_HELP },
			{ &Settings::fPickupBarX, PICKUP_BAR_X_NAME, PICKUP_BAR_X_HELP },
			{ &Settings::fPickupBarY, PICKUP_BAR_Y_NAME, PICKUP_BAR_Y_HELP },
			{ &Settings::fPickupBarLength, PICKUP_BAR_LENGTH_NAME, PICKUP_BAR_LENGTH_HELP },
			{ &Settings::fPickupBarThickness, PICKUP_BAR_THICKNESS_NAME, PICKUP_BAR_THICKNESS_HELP },
			{ &Settings::bPickupBarShowsWord, PICKUP_BAR_WORD_NAME, PICKUP_BAR_WORD_HELP },
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
