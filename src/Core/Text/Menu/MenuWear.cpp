#include "Core/Text/Lines.h"

#include "Core/Settings.h"

// The names and help lines of the wear speeds, the first Balance numbers on
// the MCM page, see MenuLine in Text.h. Each NPC row follows the player's,
// the automatic weapon row follows both weapon rows, and the companion and
// settler rows come last, their weapon rows before their armor rows.
namespace Text
{
	namespace
	{
		constexpr MenuRow ROWS[]{
			{ &Settings::fWearRateMult, WEAR_RATE_NAME, WEAR_RATE_HELP },
			{ &Settings::fNpcWearRateMult, NPC_WEAR_RATE_NAME, NPC_WEAR_RATE_HELP },
			{ &Settings::fAutoWearRateMult, AUTO_WEAR_RATE_NAME, AUTO_WEAR_RATE_HELP },
			{ &Settings::fArmorWearRateMult, ARMOR_WEAR_RATE_NAME, ARMOR_WEAR_RATE_HELP },
			{ &Settings::fNpcArmorWearRateMult, NPC_ARMOR_WEAR_RATE_NAME, NPC_ARMOR_WEAR_RATE_HELP },
			{ &Settings::fCompanionWearRateMult, COMPANION_WEAR_RATE_NAME, COMPANION_WEAR_RATE_HELP },
			{ &Settings::fSettlerWearRateMult, SETTLER_WEAR_RATE_NAME, SETTLER_WEAR_RATE_HELP },
			{ &Settings::fCompanionArmorWearRateMult, COMPANION_ARMOR_WEAR_RATE_NAME, COMPANION_ARMOR_WEAR_RATE_HELP },
			{ &Settings::fSettlerArmorWearRateMult, SETTLER_ARMOR_WEAR_RATE_NAME, SETTLER_ARMOR_WEAR_RATE_HELP },
		};
	}

	std::span<const MenuRow> MenuWear()
	{
		return ROWS;
	}
}
