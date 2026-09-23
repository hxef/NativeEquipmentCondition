#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <string>
#include <string_view>

// The sentences the plugin puts on the screen itself. Most of what it draws is
// a $KEY the game translates. The rest has no key, so it lives here in every
// language the game is sold in, picked by sLanguage in Fallout4.ini and falling
// back to English.
//
// A new sentence is one table in Text.cpp and one function here.
namespace Text
{
	// "Combat Rifle is at 62% condition. Repair it to:"
	[[nodiscard]] std::string RepairQuestion(std::string_view a_name, std::uint32_t a_percent);

	// "Gun Nut 4 reduces the components required for this repair by 50%.", said
	// over the question above when the player holds a rank of the perk pricing
	// the weapon.
	[[nodiscard]] std::string RepairDiscount(std::string_view a_perk, std::uint32_t a_rank,
		std::uint32_t a_percent);

	// "Condition must be at least 80% to modify.", or, where the floor is at
	// full, "Condition must be full to modify."
	[[nodiscard]] std::string TooDamaged(std::uint32_t a_floor);

	// "MEND", the bench's REPAIR button over a weapon worn so little that
	// repairing it is free. See Workbench/Bench.h.
	[[nodiscard]] std::string MendButton();

	// "Weapon mended.", said when MEND repairs a weapon on the spot for free.
	[[nodiscard]] std::string Mended();

	// "80% for 240 caps", one button of the question a trader asks, see
	// VendorRepair.h.
	[[nodiscard]] std::string RepairPrice(std::uint32_t a_level, std::uint32_t a_caps);

	// "Repaired to 100% for 240 caps.", said once the caps have changed hands.
	[[nodiscard]] std::string RepairPaid(std::uint32_t a_level, std::uint32_t a_caps);

	// "This trader can repair up to 80% condition.", said over the question a
	// trader asks. At 100 it says "This trader can restore weapons to full
	// condition." instead, since "up to 100%" reads as a limit.
	[[nodiscard]] std::string RepairUpTo(std::uint32_t a_ceiling);

	// "This trader can't repair past 30%.", said when REPAIR is pressed on a
	// weapon already past what the trader can do.
	[[nodiscard]] std::string RepairCeiling(std::uint32_t a_ceiling);

	// "Not enough caps to repair.", said when REPAIR is pressed and the player
	// cannot pay for even the smallest step.
	[[nodiscard]] std::string RepairUnaffordable();

	// "Weapons built mostly from this perk's mods take 25% fewer components to
	// repair at the workbench.", added to every rank of a crafting perk that
	// takes something off a repair, with what that rank alone is worth. See
	// CraftingPerks.h.
	[[nodiscard]] std::string PerkDiscount(std::uint32_t a_percent);

	// "Weapons lose condition as they are used.", a tip the loading screen
	// shows now and then. See LoadingTips.h.
	[[nodiscard]] std::string WearTip();

	// What sLanguage says, lowercased, for the log. The game reads it from
	// Fallout4.ini only after the plugin loads, so a Load asks, not an Install.
	[[nodiscard]] std::string Language();
}
