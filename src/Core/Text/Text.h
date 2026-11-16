#pragma once

#include "Core/Plugin.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Parts.h"
#include "Core/Pieces.h"
#include "Core/Text/Repair.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The sentences the plugin puts on the screen itself. Most of what it draws is
// a $KEY the game translates. The rest has no key, so NEC brings its own: 1
// file per language in F4SE/Plugins/NEC/Translations, picked by sLanguage in
// the player's Fallout4.ini, with the English compiled in for any line a file
// lacks, see Translations.cpp.
//
// A new sentence is 1 line in each of the 12 files of publish/Translations,
// with its note in en.ini, and 1 function in Repair.cpp, for the repair
// question and the bench, declared in Repair.h, or in Trader.cpp, for what a
// trader says, in Tips.cpp, for the loading screen, or in Text.cpp, for the
// rest, declared here. A sentence that names the kind of item takes a bool, or
// a Trade where a trader tells clothing from armor, since Core cannot ask
// Condition which kind it is.
// An option of the MCM page is a row in MenuSwitches.cpp, MenuWear.cpp,
// MenuNumbers.cpp, MenuHudLog.cpp or MenuPresets.cpp, which MenuLine finds,
// with no function of its own. A part's name is a row in PartsPlay.cpp,
// PartsLoot.cpp or PartsUi.cpp, a piece's in PiecesPlay.cpp, PiecesArmor.cpp
// or PiecesUi.cpp, and its phrase in Phrases.cpp. A grey line of the MCM page
// is said in MenuNotes.cpp, or in MenuPieces.cpp where it names pieces.
namespace Text
{
	// Lets NEC read the language files, once game data is ready, and reads
	// the player's now, so NEC.log names it as the game starts. Before this
	// every line is English. A later change of sLanguage reads that
	// language's file the first time a line is needed.
	void Load();

	// What a trader deals in, named in what a trader says. A trader repairs
	// each kind as far as their stock of it allows, see VendorRepair/Stock.h.
	enum class Trade
	{
		kWeapons,
		kArmor,
		kClothing,
	};

	// Takes the barter screen's word for caps as the screen opens, a_word as
	// the movie translated "$CAPS". When a mod renamed the currency, the 3
	// lines below that name caps use that word until the screen closes: "240
	// tickets".
	void SetCurrency(std::string_view a_word);

	// Back to the language file's word for caps, as the barter screen closes.
	void ForgetCurrency();

	// "240 caps", or "1 cap", the column after the level in a row of the
	// question a trader asks, see VendorRepair.h.
	[[nodiscard]] std::string RepairCaps(std::uint32_t a_caps);

	// "Repaired to 100% for 240 caps.", said once the caps have changed hands.
	[[nodiscard]] std::string RepairPaid(std::uint32_t a_level, std::uint32_t a_caps);

	// "This trader can repair clothing up to 40% condition.", said over the
	// question a trader asks. At 100 it says "This trader can restore clothing
	// to full condition." instead, since "up to 100%" reads as a limit.
	[[nodiscard]] std::string RepairUpTo(std::uint32_t a_ceiling, Trade a_trade);

	// "This trader can repair clothing only up to 40%.", said when REPAIR is
	// pressed on an item the trader can bring no further for its kind.
	[[nodiscard]] std::string RepairCeiling(std::uint32_t a_ceiling, Trade a_trade);

	// "Not enough caps to repair.", said when REPAIR is pressed and the player
	// cannot pay for even the smallest step.
	[[nodiscard]] std::string RepairUnaffordable();

	// "Accept or reset the trade before repairing.", said when REPAIR is
	// pressed while a trade is pending, see VendorRepair/Button.h.
	[[nodiscard]] std::string RepairTradePending();

	// "Repairs up to", the title of the trader's repair sign on the barter
	// screen, see VendorRepair/Sign.h.
	[[nodiscard]] std::string SignTitle();

	// "No repairs", the sign of a trader who repairs no kind.
	[[nodiscard]] std::string SignNone();

	// "Weapons", a kind's name on the sign, which prints the level beside it.
	[[nodiscard]] std::string SignKind(Trade a_trade);

	// A crafting perk rank's sentence, see CraftingPerks.h and en.ini: the
	// discount a_off on weapons, armor or both by a_weapons and a_armor, joined
	// to the repair lock's start a_from, written as 70.5 or 50 (empty is any
	// condition), else either one alone, else nothing. Each form is a key of
	// its own, so every language joins the 2 its own way.
	[[nodiscard]] std::string PerkRepairs(std::uint32_t a_off, bool a_weapons, bool a_armor, const std::optional<std::string>& a_from);

	// "Spares used to repair weapons and armor at the workbench restore more
	// condition.", added to every rank of Scrapper while spare repair is on.
	[[nodiscard]] std::string PerkScrapper();

	// "Combat Armor Chest Piece is worn out.", said in the corner when a piece
	// the player wears reaches 0.
	[[nodiscard]] std::string ArmorWornOut(std::string_view a_name);

	// "Spoiled food", what a chemistry station's Fertilizer row needs, shown
	// with its count as the station shows a component. See
	// FoodSpoil/Fertilizer.cpp.
	[[nodiscard]] std::string SpoiledFood();

	// The tips the loading screen shows now and then, on wear, the bench, a
	// trader's repairs, broken gear, jams, loot and food. See LoadingTips.h.
	// BenchTip tells of supplies while a_supplies, and of the mod lock at full.
	[[nodiscard]] std::string WearTip();
	[[nodiscard]] std::string BenchTip(std::uint32_t a_modifyFloor, bool a_supplies);
	[[nodiscard]] std::string TraderTip();
	[[nodiscard]] std::string BrokenTip();
	[[nodiscard]] std::string JamTip();
	[[nodiscard]] std::string LootTip();
	[[nodiscard]] std::string FoodTip();

	// What a help line's id adds to its setting's key on the MCM page.
	inline constexpr std::string_view MENU_HELP = ".help";

	// A line of the MCM page by its id: a setting's key for its name, the key
	// and MENU_HELP for its help, a section's id from SECTIONS in Text.cpp,
	// such as Wear, for its title, or version for the first line of NEC's main
	// page, "NEC 1.7.1 on Fallout 4 1.11.240". Empty for an id it does not
	// know. Each row of the word tables points at its setting and takes the key
	// from it.
	[[nodiscard]] std::string MenuLine(std::string_view a_id);

	// a_help, a HUD bar slider's help, then "Default: 1170." with the bar's
	// default place on the page.
	[[nodiscard]] std::string MenuHudBarDefault(std::string_view a_help, std::int32_t a_number);

	// A part's name, "Workbench repairs", in the player's language.
	[[nodiscard]] std::string PartName(Part a_part);

	// The same in English, for NEC.log.
	[[nodiscard]] std::string_view PartLogName(Part a_part);

	// Who reads a line: the player on the MCM page, in their language, or
	// NEC.log, in English with every piece named. PartLogName and the English
	// lines never ask the language setting, so they are safe during install and
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
	// off, since Gun wear from firing is left to ...", "...: off, as this
	// game version (1.11.191) is not supported." or on OG "...: off on
	// this game version (1.10.163)."
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
	// pieces a part line on the page names as off, in this note or another
	// block's, which the page leaves out, with no sentence when none is left.
	[[nodiscard]] std::vector<std::string> SettingLines(const SettingLink& a_link, const CallPatch::Effect& a_effect,
		Out a_out = Out::kPage, std::span<const Piece> a_named = {});

	// The line a block of the page ends with, "No effect for now: Workbench
	// repair cost.", and a setting's English name for NEC.log.
	[[nodiscard]] std::string MenuNoEffect(std::span<const std::string> a_names, Out a_out = Out::kPage);
	[[nodiscard]] std::string SettingLogName(const Settings::Named& a_setting);

	// The list at the top of NEC's main page: its first line, shown while a
	// mod has a part, 1 line per mod with the parts it has, "Left to
	// NECClashTest.dll: CND on item cards (containers and traders), Workbench
	// repairs.", and the line that ends a list too long to show whole.
	[[nodiscard]] std::string MenuTopIntro();
	[[nodiscard]] std::string MenuTopOwner(const CallPatch::Owner& a_owner, std::span<const std::string> a_parts);
	[[nodiscard]] std::string MenuTopMore(std::size_t a_count);

	// The help line under every grey row, and the one shown instead while
	// only this game version keeps parts off and no mod has any, which on
	// OG says those parts do not work there.
	[[nodiscard]] std::string MenuNoteHelp();
	[[nodiscard]] std::string MenuVersionHelp();

	// "NEC.log names the rest.", the last line of a grey row cut to fit the
	// page.
	[[nodiscard]] std::string MenuNoteRest();

	// "A, B and C" in the player's language, or in English for NEC.log.
	[[nodiscard]] std::string List(std::span<const std::string> a_items, Out a_out = Out::kPage);

	// The language the player's lines are in: sLanguage lowercased, "en"
	// before Load, and the LineWidth and RowLines its file's [Layout] sets, 0
	// where it sets none. See UI/Mcm/Fit.cpp.
	struct Layout
	{
		std::string_view code;
		std::size_t      lineWidth = 0;
		std::size_t      rowLines = 0;
	};

	[[nodiscard]] Layout LayoutNow();
}
