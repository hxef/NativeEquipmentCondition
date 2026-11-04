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
// a $KEY the game translates. The rest has no key, so NEC brings its own: 1
// file per language in F4SE/Plugins/NEC/Translations, picked by sLanguage in
// the player's Fallout4.ini, with the English compiled in for any line a file
// lacks, see Translations.cpp.
//
// A new sentence is 1 line in each of the 12 files of publish/Translations,
// with its note in en.ini, and 1 function in Repair.cpp, for the repair
// question and the bench, in Trader.cpp, for what a trader says, in Tips.cpp,
// for the loading screen, or in Text.cpp, for the rest, declared here. A
// sentence that names the kind of item takes a bool, or a Trade where a trader
// tells clothing from armor, since Core cannot ask Condition which kind it is.
// An option of the MCM page is a row in MenuSwitches.cpp, MenuWear.cpp,
// MenuNumbers.cpp or MenuHudLog.cpp, which MenuLine finds, with no function of
// its own. A part's name is a row in PartsPlay.cpp, PartsLoot.cpp or
// PartsUi.cpp, a piece's in PiecesPlay.cpp, PiecesArmor.cpp or PiecesUi.cpp,
// and its phrase in Phrases.cpp. A grey line of the MCM page is said in
// MenuNotes.cpp, or in MenuPieces.cpp where it names pieces.
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

	// "Combat Rifle is at 62% condition. Repair it to:"
	[[nodiscard]] std::string RepairQuestion(std::string_view a_name, std::uint32_t a_percent);

	// "Repair 10mm Pistol to 70%?", the title of the bench's repair box.
	[[nodiscard]] std::string RepairBoxTitle(std::string_view a_name, std::uint32_t a_percent);

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

	// "USE A SPARE", the top row of the bench's repair box while the player
	// carries a spare, see Workbench/Spares.h.
	[[nodiscard]] std::string SpareButton();

	// "Hunting Rifle is at 85% condition. Use up a spare to repair it:"
	[[nodiscard]] std::string SpareQuestion(std::string_view a_name, std::uint32_t a_percent);

	// A spare's name with its marks before it in the order equipped, favorite,
	// legendary, each a word in brackets, since no language's font for the box
	// has a star or a heart:
	// "(equipped) Hunting Rifle", "(favorite) (legendary) Hunting Rifle".
	[[nodiscard]] std::string SpareName(std::string_view a_name, bool a_equipped, bool a_favorite, bool a_legendary);

	// "Hunting Rifle (60%)  +7%", 1 row of the spare box, a_name from
	// SpareName.
	[[nodiscard]] std::string SpareRow(std::string_view a_name, std::uint32_t a_percent, std::uint32_t a_gain);

	// "Salvaged Steel, Screw (2) and Spring.", the parts a spare gave back,
	// each with its count past 1.
	[[nodiscard]] std::string SpareSalvaged(std::span<const std::string> a_parts);

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
	// The bench tip tells of the mod lock only where a_modifyFloor is at full.
	[[nodiscard]] std::string WearTip();
	[[nodiscard]] std::string BenchTip(std::uint32_t a_modifyFloor);
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
