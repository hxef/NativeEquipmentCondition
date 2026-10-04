#pragma once

#include "Core/Plugin.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Parts.h"
#include "Core/Pieces.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The sentences the plugin puts on the screen itself. Most of what it draws is
// a $KEY the game translates. The rest has no key, so it lives here in every
// language the game is sold in, picked by sLanguage in Fallout4.ini and falling
// back to English.
//
// A new sentence is one table in Repair.cpp, for the repair question and the
// bench, in Trader.cpp, for what a trader says, in Tips.cpp, for the loading
// screen, or in Text.cpp, for the rest, and one function here. A sentence that
// names the kind of item takes a bool, or a Trade where a trader tells
// clothing from armor, since Core cannot ask Condition which kind it is. An
// option of the MCM page is a row in MenuSwitches.cpp, MenuNumbers.cpp or
// MenuHudLog.cpp, which MenuLine finds, with no function of its own. A part's
// name is a table in PartsPlay.cpp or PartsUi.cpp, a piece's in PiecesPlay.cpp,
// PiecesArmor.cpp or PiecesUi.cpp. A grey line of the MCM page is a table in
// MenuNotes.cpp, or in MenuPieces.cpp where it names pieces.
namespace Text
{
	// What a trader deals in, named in what a trader says. A trader repairs
	// each kind as far as their stock of it allows, see VendorRepair/Stock.h.
	enum class Trade
	{
		kWeapons,
		kArmor,
		kClothing,
	};

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

	// "This trader can repair clothing up to 40% condition.", said over the
	// question a trader asks. At 100 it says "This trader can restore clothing
	// to full condition." instead, since "up to 100%" reads as a limit.
	[[nodiscard]] std::string RepairUpTo(std::uint32_t a_ceiling, Trade a_trade);

	// "This trader can't repair clothing past 40%.", said when REPAIR is
	// pressed on an item already past what the trader can do for its kind.
	[[nodiscard]] std::string RepairCeiling(std::uint32_t a_ceiling, Trade a_trade);

	// "Not enough caps to repair.", said when REPAIR is pressed and the player
	// cannot pay for even the smallest step.
	[[nodiscard]] std::string RepairUnaffordable();

	// "Accept or reset the trade before repairing.", said when REPAIR is
	// pressed while a trade is pending, see VendorRepair/Button.h.
	[[nodiscard]] std::string RepairTradePending();

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

	// A line of the MCM page by its id: a setting's key for its name, the key
	// and ".help" for its help, or a section's id, Wear, Weapons, Loot,
	// Repairs, HUD, Extras or Log, for its title. Empty for an id it does not
	// know. Each row of the word tables points at its setting and takes the
	// key from it.
	[[nodiscard]] std::string MenuLine(std::string_view a_id);

	// A part's name, "Workbench repairs", in the player's language.
	[[nodiscard]] std::string PartName(Part a_part);

	// The same in English, for NEC.log.
	[[nodiscard]] std::string_view PartLogName(Part a_part);

	// Who reads a line: the player on the MCM page, in their language, or
	// NEC.log, in English with every piece named. PartLogName and the English
	// rows never ask the game's language, so they are safe during install and
	// under a lock.
	enum class Out
	{
		kPage,
		kLog,
	};

	// The grey lines of the MCM page about a part with pieces off, 1 a cause,
	// the last saying what still works: "Worn armor protection (how NPCs rank
	// armor): off, left to NECClashTest.dll. Still works: damage resistance,
	// energy and radiation resistance, best item marks in menus.", "Jamming:
	// off, since Gun wear from firing is left to ...", or "...: off, as this
	// game version (1.10.984) is not supported."
	[[nodiscard]] std::vector<std::string> PartLines(const CallPatch::Loss& a_loss, Out a_out = Out::kPage);

	// A part with the pieces of it that are off, "CND on item cards
	// (containers and traders)", or the part alone when they are all of it.
	// Then the pieces alone, "containers and traders, cooking and chemistry
	// stations".
	[[nodiscard]] std::string PartPieces(Part a_part, std::span<const Piece> a_pieces, Out a_out = Out::kPage);
	[[nodiscard]] std::string PieceNames(std::span<const Piece> a_pieces, Out a_out = Out::kPage);

	// The lines of a setting with some pieces off: "Weapon wear speed: no
	// effect on gun wear from firing for now. Still changes melee wear.", and
	// "Repair prices follow the item's barter price for now." while Worn item
	// prices is off. Empty while it works in full or is idle. a_named are
	// pieces a part line right above names as off, which the page leaves out,
	// with no sentence when none is left.
	[[nodiscard]] std::vector<std::string> SettingLines(const SettingLink& a_link, const CallPatch::Effect& a_effect,
		Out a_out = Out::kPage, std::span<const Piece> a_named = {});

	// The line a block of the page ends with, "No effect for now: Workbench
	// repair cost.", and a setting's English name for NEC.log.
	[[nodiscard]] std::string MenuNoEffect(std::span<const std::string> a_names, Out a_out = Out::kPage);
	[[nodiscard]] std::string SettingLogName(const Settings::Named& a_setting);

	// The list at the top of the page: its first line, shown while a mod has
	// a part, 1 line per mod with the parts it has, "Left to NECClashTest.dll:
	// CND on item cards (containers and traders), Workbench repairs.", and the
	// line that ends a list too long to show whole.
	[[nodiscard]] std::string MenuTopIntro();
	[[nodiscard]] std::string MenuTopOwner(const CallPatch::Owner& a_owner, std::span<const std::string> a_parts);
	[[nodiscard]] std::string MenuTopMore(std::size_t a_count);

	// The help line under every grey row, and the one shown instead while
	// only this game version keeps parts off and no mod has any.
	[[nodiscard]] std::string MenuNoteHelp();
	[[nodiscard]] std::string MenuVersionHelp();

	// "NEC.log names the rest.", the last line of a grey row cut to fit the
	// page.
	[[nodiscard]] std::string MenuNoteRest();

	// "A, B and C" in the player's language, or in English for NEC.log.
	[[nodiscard]] std::string List(std::span<const std::string> a_items, Out a_out = Out::kPage);

	// What sLanguage says, lowercased, for the log. The game reads it from
	// Fallout4.ini only after the plugin loads, so a Load asks, not an Install.
	[[nodiscard]] std::string Language();
}
