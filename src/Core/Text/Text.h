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
// A new sentence is one table in Repair.cpp, for the repair menus, in
// Tips.cpp, for the loading screen, or in Text.cpp, for the rest, and one
// function here. A sentence that names the kind of item takes a bool, since
// Core cannot ask Condition which kind it is.
namespace Text
{
	// "Combat Rifle is at 62% condition. Repair it to:"
	[[nodiscard]] std::string RepairQuestion(std::string_view a_name, std::uint32_t a_percent);

	// "Gun Nut 4 reduces the components required for this repair by 50%.", said
	// over the question above when the player holds a rank of the perk pricing
	// the item.
	[[nodiscard]] std::string RepairDiscount(std::string_view a_perk, std::uint32_t a_rank,
		std::uint32_t a_percent);

	// "Condition must be at least 80% to modify.", or, where the floor is at
	// full, "Condition must be full to modify."
	[[nodiscard]] std::string TooDamaged(std::uint32_t a_floor);

	// "MEND", the bench's REPAIR button over an item worn so little that
	// repairing it is free. See Workbench/Bench.h.
	[[nodiscard]] std::string MendButton();

	// "Weapon mended.", or "Armor mended." with a_armor, said when MEND repairs
	// an item on the spot for free.
	[[nodiscard]] std::string Mended(bool a_armor);

	// "80% for 240 caps", one button of the question a trader asks, see
	// VendorRepair.h.
	[[nodiscard]] std::string RepairPrice(std::uint32_t a_level, std::uint32_t a_caps);

	// "Repaired to 100% for 240 caps.", said once the caps have changed hands.
	[[nodiscard]] std::string RepairPaid(std::uint32_t a_level, std::uint32_t a_caps);

	// "This trader can repair up to 80% condition.", said over the question a
	// trader asks. At 100 it says "This trader can restore this item to full
	// condition." instead, since "up to 100%" reads as a limit.
	[[nodiscard]] std::string RepairUpTo(std::uint32_t a_ceiling);

	// "This trader can't repair past 30%.", said when REPAIR is pressed on an
	// item already past what the trader can do.
	[[nodiscard]] std::string RepairCeiling(std::uint32_t a_ceiling);

	// "Not enough caps to repair.", said when REPAIR is pressed and the player
	// cannot pay for even the smallest step.
	[[nodiscard]] std::string RepairUnaffordable();

	// "This item can't be repaired at a workbench. A trader can restore it.",
	// said when REPAIR is pressed on an item the bench has nothing to rebuild
	// from, see Workbench/Cost.h.
	[[nodiscard]] std::string BenchCannotRepair();

	// "This item can't be modified.", said at the bench for an item no mod
	// fits, once a repair brings it to full and whenever its mod slots are
	// asked for. See Workbench/Display.h.
	[[nodiscard]] std::string CannotModify();

	// "Weapons built mostly from this perk's mods take 25% fewer components to
	// repair at the workbench.", added to every rank of a crafting perk that
	// takes something off a repair, with what that rank alone is worth.
	// a_weapons and a_armor say what the perk's mods go on: armor alone reads
	// "Armor built mostly ... takes", both read "Weapons and armor built
	// mostly ... take". See CraftingPerks.h.
	[[nodiscard]] std::string PerkDiscount(std::uint32_t a_percent, bool a_weapons, bool a_armor);

	// "Combat Armor Chest Piece is worn out.", said in the corner when a piece
	// the player wears reaches 0.
	[[nodiscard]] std::string ArmorWornOut(std::string_view a_name);

	// The tips the loading screen shows now and then, on wear, the bench, a
	// trader's repairs, broken gear, jams and loot. See LoadingTips.h.
	[[nodiscard]] std::string WearTip();
	[[nodiscard]] std::string BenchTip();
	[[nodiscard]] std::string TraderTip();
	[[nodiscard]] std::string BrokenTip();
	[[nodiscard]] std::string JamTip();
	[[nodiscard]] std::string LootTip();

	// What sLanguage says, lowercased, for the log. The game reads it from
	// Fallout4.ini only after the plugin loads, so a Load asks, not an Install.
	[[nodiscard]] std::string Language();
}
