#pragma once

#include "Condition/Condition.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Parts.h"
#include "Core/Pieces.h"
#include "Core/Plugin.h"
#include "Core/Settings/Settings.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// What the files of the folder share. Private to this folder.
namespace Mcm
{
	using Params = Scaleform::GFx::FunctionHandler::Params;

	// config.json's modName, which MCM also puts on every entry of NEC's pages.
	inline constexpr std::string_view MOD = "NEC"sv;

	// A hidden switch that hides rows while a number is at its off value, as a
	// switch that is off hides the rows under it, or while a few settings are.
	// Each has config.json's refreshMenu, so the rows hide and come back as it
	// moves. rows are the settings of the rows it hides, which their block's
	// note leaves out too, and nullptr after the last. A row on 2 hides shows
	// only while both do, as config.json joins their numbers with AND.
	struct Hide
	{
		std::string_view                      id;
		bool                                  (*shows)();
		std::array<const Settings::Named*, 8> rows;
	};

	// Whether anything repairs as a workbench does: the bench itself, or
	// REPAIR on the Pip-Boy's inspect screen, iPipboyRepair 1 or 2 as set.
	// When neither does, only traders repair.
	[[nodiscard]] inline bool BenchWayRepairs()
	{
		const auto pipboy = Settings::iPipboyRepair.GetValue();
		return Settings::bWorkbenchRepair.GetValue() || pipboy == 1 || pipboy == 2;
	}

	// Jams start at shows while the jam chance is above 0, Perk lock and Lock
	// spares too while Lock repairs is not Off, and Supplies only from while
	// Servicing supplies is above 0. An iRepairLock of 3 or -1 locks as By
	// its mods does, so only 0 hides them. The 8 rows that tune paid repairs
	// show while Workbench repair cost is above 0, since at 0 every repair is
	// a free MEND and no repair box opens, see FreeRepairs in
	// UI/Repair/Workbench/Bench.h, and while something repairs as a workbench
	// does. The Maintenance header hides with them, as no row is left under
	// it. Workbench repair cost and the Repair with spares section show only
	// while something repairs so too, and Repair box opens on only while a
	// levels box can open, at the bench or as Workbench in the Pip-Boy. Lock
	// spares too also hides while Spare repair is off, by its config.json
	// row, and so do Keep spare mods and Spare mod rows. Spare mod rows shows
	// only while Keep spare mods opens a box, at Choose mods to use or Choose
	// mods to keep. The Penalty mark section shows while either penalty point
	// marks a bar, from 0.01 to 0.99. Its 2 points sit on the Weapons and the
	// Armor page, so its id names the first, and both sliders refresh the
	// menu.
	inline constexpr Hide HIDES[]{
		{ "fJamChance:Jams"sv, [] { return Settings::fJamChance.GetValue() > 0.0F; }, { &Settings::fJamStart } },
		{ "iRepairLock:Lock"sv, [] { return Settings::iRepairLock.GetValue() != 0; }, { &Settings::iPerkLock, &Settings::bLockSpares } },
		{ "iRepairSupplies:Supplies"sv, [] { return Settings::iRepairSupplies.GetValue() > 0; }, { &Settings::fSuppliesOnlyFrom } },
		{ "bWorkbenchRepair:Levels"sv, [] { return Settings::bWorkbenchRepair.GetValue() || Settings::iPipboyRepair.GetValue() == 2; },
			{ &Settings::iRepairBoxRow } },
		{ "bWorkbenchRepair:Repairs"sv, BenchWayRepairs,
			{ &Settings::fBenchCostMult, &Settings::bSpareRepair, &Settings::fSpareWorthMult, &Settings::iUnscrappableSpares,
				&Settings::iKeepSpareMods, &Settings::iSpareModRows } },
		{ "iKeepSpareMods:Choose"sv, [] { return Settings::KeepSpareModsAsks(Settings::iKeepSpareMods.GetValue()); },
			{ &Settings::iSpareModRows } },
		{ "fBenchCostMult:Paid"sv, [] { return Settings::fBenchCostMult.GetValue() > 0.0F && BenchWayRepairs(); },
			{ &Settings::fFreeMendFrom, &Settings::iRepairSupplies, &Settings::fSuppliesOnlyFrom, &Settings::iRepairLock,
				&Settings::iPerkLock, &Settings::iPerkComponentDiscount, &Settings::bLockSpares, &Settings::iRepairBoxRow } },
		{ "fWeaponPenaltyStart:Marks"sv, [] { return Condition::StartPercent(Condition::Kind::kWeapon) != 0 || Condition::StartPercent(Condition::Kind::kArmor) != 0; },
			{ &Settings::fPenaltyMarkWidth, &Settings::bPenaltyMarkAsLine } },
	};

	// The hide a_id names, or nullptr.
	[[nodiscard]] constexpr const Hide* HideOf(std::string_view a_id)
	{
		const auto it = std::ranges::find(HIDES, a_id, &Hide::id);
		return it != std::end(HIDES) ? &*it : nullptr;
	}

	// NEC's answers on root.mcm, in Bridge.cpp, each for a call that is NEC's.
	// a_id is a value's id, bSpawnCondition:Features or the hidden
	// bSpawnCondition:Free, or for GetFullName a line's id, bSpawnCondition,
	// bSpawnCondition.help or bSpawnCondition.note.
	void GetBool(const Params& a_params, std::string_view a_id);
	void GetInt(const Params& a_params, std::string_view a_id);
	void GetFloat(const Params& a_params, std::string_view a_id);
	void SetBool(const Params& a_params, std::string_view a_id);
	void SetInt(const Params& a_params, std::string_view a_id);
	void SetFloat(const Params& a_params, std::string_view a_id);
	void GetFullName(const Params& a_params, std::string_view a_id);

	// How an answer is handed back, in Bridge.cpp: a value, a text, which the
	// movie gets a copy of, and the warning for an id NEC does not know,
	// said once for each.
	template <class T>
	void Answer(const Params& a_params, T a_value)
	{
		if (a_params.retVal) {
			*a_params.retVal = a_value;
		}
	}

	void AnswerText(const Params& a_params, const std::string& a_text);
	void Unknown(std::string_view a_id);

	// The presets rows of NEC's main page, see Core/Presets/Presets.h. The
	// Preset list picks by its place in PRESET_LIST, the Save as new preset
	// text field saves a new preset, the Describe preset text field writes
	// the picked own preset's description, and the 2 hidden switches show
	// Reset preset while the picked preset has changes and Delete preset and
	// Describe preset while it is the player's own. A 3rd shows the grey line
	// under Preset while the pick has a description, and PRESET_ABOUT_NOTE is
	// that line's id and the id of its words.
	// Their answers are in PresetRows.cpp, and a_id of GetList is the id of a
	// list, presets. PickPreset answers SetInt for PRESET_ID.
	void GetString(const Params& a_params, std::string_view a_id);
	void SetString(const Params& a_params, std::string_view a_id);
	void GetList(const Params& a_params, std::string_view a_id);
	void PickPreset(const Params& a_params);

	inline constexpr std::string_view PRESET_ID = "iPreset:Presets"sv;
	inline constexpr std::string_view PRESET_LIST = "presets"sv;
	inline constexpr std::string_view PRESET_SAVE_ID = "sSavePreset:Presets"sv;
	inline constexpr std::string_view PRESET_DESCRIBE_ID = "sDescribePreset:Presets"sv;
	inline constexpr std::string_view PRESET_CHANGES_ID = "changes:Presets"sv;
	inline constexpr std::string_view PRESET_OWN_ID = "own:Presets"sv;
	inline constexpr std::string_view PRESET_ABOUT_ID = "about:Presets"sv;
	inline constexpr std::string_view PRESET_ABOUT_NOTE = "preset.about"sv;

	// MCM draws a noTint text entry white, so this colour shows as is.
	// The tag quotes it with " as MCM's own html does.
	inline constexpr std::string_view GREY = "#808080"sv;

	// The grey html of the line under Preset, empty for none, in
	// PresetRows.cpp.
	[[nodiscard]] std::string AboutLine();

	// AboutLine as the row's words: 1 space for no line, since MCM keeps the
	// id as the words of an empty answer. The row hides then anyway.
	[[nodiscard]] std::string AboutWords();

	// Writes that line on the row while NEC's main page shows, true when it
	// wrote, see Layout.cpp.
	bool AboutRow(Scaleform::GFx::Value& a_entries);

	// NEC's box with vanilla's OK, saying a_body, after a save, a delete or a
	// description of a preset that failed and so changed nothing, in
	// PresetButtons.cpp.
	void SayFailed(const std::string& a_body);

	// Asks the game to work out again what it keeps from a_setting, whose
	// value just changed, by the page or by a pick, a reset or a delete of a
	// preset, in Bridge.cpp.
	void Followed(const Settings::Named& a_setting);

	// What the hidden switches of the page's grey rows and of HIDES say, and
	// what those grey rows read, in Notes.cpp. a_id is parts:Note,
	// bSpawnCondition:Free, bSpawnCondition:Note, fDamageFloor:Note, an id of
	// HIDES such as fJamChance:Jams or a preset switch for Shows, and
	// parts.note, bSpawnCondition.note, fDamageFloor.note, parts.help or
	// preset.about for Note. Nothing for any other id.
	[[nodiscard]] std::optional<bool>        Shows(std::string_view a_id);
	[[nodiscard]] std::optional<std::string> Note(std::string_view a_id);

	// What Notes.cpp and TopList.cpp share: a piece a grey line names as left
	// to an owner, a block's note from BlockOf, in Notes.cpp, and the list at
	// the top from TopLines, in TopList.cpp, with OnlyGameVersion.
	struct Left
	{
		Piece            piece;
		CallPatch::Owner owner;

		bool operator==(const Left&) const = default;
	};

	// A block's note and what its part lines name, of the lines it shows.
	struct Block
	{
		std::vector<std::string> lines;
		std::vector<Left>        left;
	};

	[[nodiscard]] Block                    BlockOf(const SettingLink& a_lead);
	[[nodiscard]] std::vector<std::string> TopLines();
	[[nodiscard]] bool                     OnlyGameVersion();

	// How tall a grey row may be, in Fit.cpp: LinesOf, the lines a_line takes
	// on the page in the player's language, and Capped, a_lines cut to fit.
	// a_shorter builds line a_line again in a_room lines or fewer, or nothing
	// where it has no such form.
	using Shorter = std::function<std::optional<std::string>(std::size_t a_line, std::size_t a_room)>;

	struct Kept
	{
		std::vector<std::string> lines;
		std::size_t              whole = 0;  // how many of the first lines are kept as they were
	};

	[[nodiscard]] std::size_t LinesOf(std::string_view a_line);
	[[nodiscard]] Kept        Capped(std::vector<std::string> a_lines, const Shorter& a_shorter = {});

	// Notes a change from the page that its file does not have yet, in
	// Mcm.cpp. The file is NEC_custom.ini or the picked preset's, see
	// Presets::Target. a_before is kept from the first change still owed, so
	// a dragged slider is 1 write and 1 change line. a_default is the value
	// at which the key's line goes from the file instead, see Presets::Base.
	void Owe(std::string_view a_section, std::string_view a_key, std::string_view a_before, std::string_view a_now, std::string_view a_default);

	// Writes every change still owed at once, before a pick, a save, a reset,
	// a delete or a description, so a setting made a moment before lands in
	// the file it was made for. a_what names the action in the trace line,
	// such as pick.
	void WriteOwed(std::string_view a_what);

	// Has the frame listener ask MCM to draw its page again on the next
	// frame, see RefreshMcm.
	void RefreshSoon();

	// What the game keeps worked out from a setting, done again on the engine's
	// task queue after a change, in FollowUps.cpp.
	enum class FollowUp : std::size_t
	{
		kCards,        // the Pip-Boy's item cards
		kResistances,  // every loaded actor's damage type resistances
		kCarried,      // what the player carries, see SpawnCondition::KeepCarried
		kFood,         // the food the player and companions carry, see FoodSpoil::StampCarried
		kPerks,        // the perk descriptions the Pip-Boy keeps, see CraftingPerks::RetellPipboy
		kFoodCards,    // the Aid cards, for food freshness rows
		kTotal,
	};

	void Queue(FollowUp a_followUp);

	// The HUD bar sliders, in HudBar.cpp: the bar's place in HUD units from
	// the screen's top left, from HudCondition::Measured. HudBarShows
	// answers bHudCondition:Placed, false until the HUD has measured the
	// bar. HudBarValue and HudBarShift answer the 2 slider ids and
	// HudBarDefault their help ids, each nothing for any other id or before
	// that measure. HudBarRanges writes the live ends and value on NEC's
	// entries, true when it wrote one. HudBarHidden says whether a_setting is
	// a slider the page hides until that measure.
	[[nodiscard]] bool                        IsHudBar(std::string_view a_id);
	[[nodiscard]] bool                        HudBarHidden(const Settings::Named& a_setting);
	[[nodiscard]] std::optional<bool>         HudBarShows(std::string_view a_id);
	[[nodiscard]] std::optional<double>       HudBarValue(std::string_view a_id);
	[[nodiscard]] std::optional<float>        HudBarShift(std::string_view a_id, float a_number);
	[[nodiscard]] std::optional<std::int32_t> HudBarDefault(std::string_view a_id);
	[[nodiscard]] bool                        HudBarRanges(Scaleform::GFx::Value& a_entries);

	// Puts MCM's settings list in None mode while a page of NEC's shows, and
	// back once another page shows, keeps the HUD bar sliders' live ends, and
	// hides NEC's pages in the left list while bNecOn is off, in Layout.cpp.
	// Called every frame of the main or pause menu.
	void Layout(Scaleform::GFx::Movie& a_movie);

	// Puts MCM's left list highlight back on the open page of NEC's when the
	// mouse left it on another page name, in Layout.cpp. Called as MCM asks
	// for a_id, an id of HIDES or a preset switch.
	void KeepOpenPage(Scaleform::GFx::Movie* a_movie, std::string_view a_id);

	// Has MCM read every row again and draw the open page, as a row with
	// refreshMenu does, with the highlight put back first as above. Nothing
	// while MCM is not open, in Layout.cpp.
	void RefreshMcm(Scaleform::GFx::Movie& a_movie);
}
