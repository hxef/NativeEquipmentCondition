#pragma once

#include "Core/Settings/Settings.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Presets, the sets of gameplay settings NEC's main MCM page picks from.
// Default is NEC.ini's values with the player's changes in NEC_custom.ini.
// Every other preset is an ini file in NEC.ini's sections that holds only the
// keys it changes. A shipped or downloaded one sits in
// Data\F4SE\Plugins\NEC\Config\MCM_Presets, and NEC never writes, renames or
// removes a file there. Everything NEC writes for presets goes in its Custom
// folder: the player's changes to a shipped preset in a file of the same name,
// and a preset of the player's own in a file of its own.
//
// The values stack. Default: NEC.ini, then NEC_custom.ini. A shipped preset:
// NEC.ini, its file, then the player's changes to it. An own preset: NEC.ini,
// then its file. So a gameplay setting a preset leaves out takes NEC.ini's
// value, and a setting no preset holds, such as what a menu shows, keeps the
// player's value from NEC_custom.ini whatever is picked.
//
// NEC_custom.ini keeps the pick as [Presets] sPreset. Nothing goes in a save.
// Files and NEC.log name presets in English, with " (Custom)" for one with
// changes, and the MCM page shows them in the player's language.
namespace Presets
{
	// Reads the picked preset over what the setting store read from NEC.ini
	// and NEC_custom.ini, once as the game starts. Runs before NEC.log opens,
	// so it logs nothing and keeps its lines for Report.
	void Load();

	// Logs what Load found, once, as the log opens.
	void Report();

	// -------------------------------------------------------------------
	// The list
	// -------------------------------------------------------------------

	enum class Kind
	{
		kDefault,
		kOriginal,  // shipped or downloaded, in MCM_Presets
		kOwn,       // saved by the player, in MCM_Presets\Custom
	};

	// 1 entry of the list.
	struct Entry
	{
		std::string name;  // the file name without .ini, _ shown as a space, or Default
		std::string stem;  // the file name without .ini, as sPreset keeps it, empty for Default
		Kind kind = Kind::kDefault;
		std::filesystem::path file;     // the preset's own file, empty for Default
		std::filesystem::path changes;  // where changes to it go, empty for an own preset
		std::size_t changed = 0;        // how many held settings that changes file sets
	};

	// Whether a preset holds a_key under [a_section], in any case. Every
	// gameplay rule is held, and NOT_HELD in Presets.cpp lists what is not.
	[[nodiscard]] bool Held(std::string_view a_section, std::string_view a_key);

	// Whether 2 file names match as Windows matches them, in any case for
	// every letter (Ä and ä, Д and д).
	[[nodiscard]] bool SameFile(std::wstring_view a_lhs, std::wstring_view a_rhs);

	// a_stem, a file name without .ini, without the NEC_ that starts the name
	// of each preset NEC ships, so NEC_New_Vegas lists as New Vegas. NEC_
	// alone stays. A player's own file loses its NEC_ too, so a preset saved
	// as NEC Hardcore Plus lists as Hardcore Plus.
	[[nodiscard]] std::string_view  Unprefixed(std::string_view a_stem);
	[[nodiscard]] std::wstring_view Unprefixed(std::wstring_view a_stem);

	// Default first, then the shipped and own presets in name order, in any
	// case. Reads both folders each time, so a file added while the game runs
	// shows at the next call. The list is kept, so an index the MCM page sends
	// back means the entry it showed.
	[[nodiscard]] std::vector<Entry> Entries();

	// The picked entry's index in the list Entries gives next, 0 for Default.
	// It reads both folders as Entries does but keeps the last list.
	[[nodiscard]] std::size_t Picked();

	// The picked preset's English name without " (Custom)", such as New Vegas.
	[[nodiscard]] std::string PickedName();

	// The picked preset's file name without .ini, empty for Default, as the
	// Settings line prints sPreset.
	[[nodiscard]] std::string PickedStem();

	// Whether the picked preset shows as (Custom): Default or a shipped preset
	// whose changes file sets a held setting.
	[[nodiscard]] bool PickedHasChanges();

	// Whether the picked preset is the player's own.
	[[nodiscard]] bool PickedIsOwn();

	// -------------------------------------------------------------------
	// A preset's description, see About.cpp
	// -------------------------------------------------------------------

	// The line a preset's own file gives under [Presets], which NEC's main
	// MCM page shows under Preset. A changes file's line is never read.
	inline constexpr Settings::Named ABOUT_KEY{ "Presets", "sDescription" };

	// What MCM's text field takes.
	inline constexpr std::size_t MOST_ABOUT_LETTERS = 100;

	// The picked preset's file name without .ini, empty for Default, its kind
	// and its file's description as written, empty for none.
	struct About
	{
		std::string stem;
		Kind kind = Kind::kDefault;
		std::string text;
	};
	[[nodiscard]] About PickedAbout();

	// a_text as the page shows it: up to its first byte that is not UTF-8,
	// each control character a space, no space at either end. Past
	// MOST_ABOUT_LETTERS letters it keeps that many, back to the last space
	// past the half when there is one, and ends in "...".
	[[nodiscard]] std::string AboutCut(std::string_view a_text);

	// How a Describe preset ended. NEC never writes a file in MCM_Presets, so
	// Default, a shipped and a downloaded preset get no description.
	enum class DescribeEnd
	{
		kKept,
		kRemoved,
		kNotOwn,      // the pick is no preset of the player's own
		kNotWritten,  // its file could not be written
	};

	// Writes a_typed as the picked own preset's description, or removes it
	// when nothing is left after the cleaning. The cleaning is AboutCut's
	// without the cut.
	DescribeEnd Describe(std::string_view a_typed);

	// -------------------------------------------------------------------
	// Where a change goes
	// -------------------------------------------------------------------

	// The file a change to a_key under [a_section] goes in: NEC_custom.ini for
	// a setting no preset holds or while Default is picked, else the picked
	// preset's file in the Custom folder.
	[[nodiscard]] std::filesystem::path Target(std::string_view a_section, std::string_view a_key);

	// The value at which a change's line goes from its file, as the MCM page
	// writes it: the picked shipped preset's own value for a held setting,
	// else NEC.ini's.
	[[nodiscard]] std::string Base(const Settings::Live<bool>& a_setting);
	[[nodiscard]] std::string Base(const Settings::Live<float>& a_setting);
	[[nodiscard]] std::string Base(const Settings::Live<std::int32_t>& a_setting);

	// What Keep writes into a_file when it is not there yet: the first line of
	// a changes file, or the whole own preset when its file was removed by
	// hand while it is picked. With a_drop, for Drop, only the latter, since a
	// Drop never makes a changes file. Empty for any other file.
	[[nodiscard]] std::string NewText(const std::filesystem::path& a_file, bool a_drop = false);

	// Whose value stands once a line goes from a_file, as NEC.log says it:
	// "the preset's" for the picked shipped preset's changes file, else
	// "NEC.ini's", as for NEC_custom.ini and an own preset.
	[[nodiscard]] std::string_view Stands(const std::filesystem::path& a_file);

	// After Keep or Drop on a_file: counts again what the picked preset's
	// changes file sets. True when the picked preset began or stopped showing
	// as (Custom), so the page draws again.
	bool Wrote(const std::filesystem::path& a_file);

	// a_file as NEC.log names it, from Data\F4SE\Plugins\NEC\Config with /, such
	// as MCM_Presets/Custom/My_Run.ini, or NEC_custom.ini.
	[[nodiscard]] std::string LogName(const std::filesystem::path& a_file);

	// -------------------------------------------------------------------
	// What the page does
	// -------------------------------------------------------------------

	// The settings whose value a pick, a reset or a delete changed, for their
	// follow ups.
	using Changed = std::vector<const Settings::Named*>;

	// Picks entry a_index of the last list, rereading its files, and keeps
	// the pick in NEC_custom.ini. The caller writes the changes the page
	// still owes first.
	Changed Pick(std::size_t a_index);

	// Drops the player's changes to the picked Default or shipped preset.
	Changed Reset();

	// Removes the picked own preset's file and picks Default. Nothing when the
	// file could not be removed, and then nothing changes.
	std::optional<Changed> Delete();

	// A save that worked, or what stopped it. A taken name stops nothing: the
	// new preset's name then ends in _2, _3 and so on.
	enum class SaveEnd
	{
		kSaved,
		kNoName,      // the typed name kept no letter or digit
		kNotWritten,  // its file could not be written
	};

	// How a save ended, and the new preset's name as the list shows it, also
	// when its file could not be written. The name is empty for kNoName.
	struct Saved
	{
		SaveEnd     end = SaveEnd::kNoName;
		std::string name;
	};

	// Saves the settings in play as a new own preset named from a_typed, and
	// picks it. a_default is Default's name in the player's language, which a
	// new preset may not take. Nothing changes unless it ends kSaved.
	[[nodiscard]] Saved Save(std::string_view a_typed, std::string_view a_default);
}
