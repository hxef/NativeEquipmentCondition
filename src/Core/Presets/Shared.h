#pragma once

#include "Core/Presets/Presets.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// What the files of the folder share. Private to this folder.
namespace Presets
{
	// -------------------------------------------------------------------
	// The files, see Files.cpp
	// -------------------------------------------------------------------

	// The 2 folders, in NEC's Config folder beside NEC.ini and NEC_custom.ini
	// (Settings.cpp), and the game runs from its own folder. Every name stays
	// wide, since a narrow path throws on a letter the system's code page lacks,
	// such as a Russian name on a Western PC.
	inline constexpr std::wstring_view ORIGINALS = L"Data/F4SE/Plugins/NEC/Config/MCM_Presets/";
	inline constexpr std::wstring_view CUSTOM = L"Data/F4SE/Plugins/NEC/Config/MCM_Presets/Custom/";

	// A bigger file is no preset, as for the list folders.
	inline constexpr std::uintmax_t MOST_BYTES = 1024 * 1024;

	// A line for NEC.log, kept until the log is open or the lock is let go.
	struct Line
	{
		bool warn = false;
		std::string text;
	};
	using Lines = std::vector<Line>;

	// Writes a_lines to NEC.log, each at info or as a warning.
	void Say(const Lines& a_lines);

	// a_folder plus a_name, a file name in UTF-8, read as UTF-8 and never in
	// the system's code page.
	[[nodiscard]] std::filesystem::path PathOf(std::wstring_view a_folder, std::string_view a_name);

	// a_file's name without .ini, in UTF-8, or "?" when it cannot be turned
	// into UTF-8.
	[[nodiscard]] std::string StemOf(const std::filesystem::path& a_file);

	// A file name without .ini as the list shows it, Unprefixed, with _ as a
	// space.
	[[nodiscard]] std::string Shown(std::string_view a_stem);

	// What both folders hold, as the list takes it.
	struct Scanned
	{
		std::vector<Entry> entries;                  // shipped and own presets, in name order
		std::vector<std::filesystem::path> orphans;  // changes files whose shipped preset is not in the list
		std::vector<std::filesystem::path> alike;    // files left out since another file lists under their name
		std::vector<std::wstring> names;             // every .ini file name without .ini in either folder
		Lines lines;                                 // what is left out and why, for the game start
	};
	[[nodiscard]] Scanned Scan();

	// Whether a_lhs comes before a_rhs in the list, by the name it lists
	// under, in any case.
	[[nodiscard]] bool Before(const Entry& a_lhs, const Entry& a_rhs);

	// Removes a_file through ::DeleteFileW, the call Mod Organizer 2's
	// virtual folder answers, then looks whether it is gone. A file already
	// gone counts as removed. False with the reason for NEC.log when the
	// call fails or the file is still there.
	bool Remove(const std::filesystem::path& a_file, std::string& a_reason);

	// Writes a new file a_file, never over one that is there. False with the
	// reason for NEC.log when the write or the move into place fails.
	bool WriteNew(const std::filesystem::path& a_file, std::string_view a_text, std::string& a_reason);

	// -------------------------------------------------------------------
	// The values, see Values.cpp and Read.cpp
	// -------------------------------------------------------------------

	// A held setting, and a value of one.
	using Setting = std::variant<Settings::Live<bool>*, Settings::Live<float>*, Settings::Live<std::int32_t>*>;
	using Value = std::variant<bool, float, std::int32_t>;

	// 1 value for each held setting, in the order of HeldSettings.
	using Values = std::vector<Value>;

	// What some files set, 1 slot for each held setting, empty where none does.
	using Given = std::vector<std::optional<Value>>;

	// Every setting a preset holds, in NEC.ini's sections and order, 53 in
	// 2.0.0.
	[[nodiscard]] std::span<const Setting> HeldSettings();
	[[nodiscard]] std::span<const Settings::Named* const> HeldNames();

	[[nodiscard]] const Settings::Named& NamedOf(const Setting& a_setting);

	// The held setting a_setting is, by its place in HeldSettings.
	[[nodiscard]] std::optional<std::size_t> HeldIndexOf(const Settings::Named& a_setting);

	// NEC.ini's values and those in play.
	[[nodiscard]] Values Defaults();
	[[nodiscard]] Value NowOf(const Setting& a_setting);

	// A value as the MCM page writes it into a file: {:g} for a float, {:d}
	// for a whole number, true or false.
	[[nodiscard]] std::string TextOf(const Value& a_value);

	// A switch left to another mod, which stays false while the game runs.
	[[nodiscard]] bool IsYielded(const Setting& a_setting);

	// What 1 file sets.
	struct Read
	{
		Given given;  // each held setting it sets with a value NEC takes
		std::size_t set = 0;
		Lines faults;       // the first 10, then 1 count line
		std::string about;  // its [Presets] sDescription as SimpleIni gives it, empty for none
	};

	// Reads a_file whole, for the held settings it sets. With a_check, each
	// text is checked first and every fault kept as a line, naming what stands
	// instead: a_stands' value of a_before, such as "NEC.ini's 0.66". Without
	// it, the file is read as the setting store reads NEC_custom.ini.
	[[nodiscard]] Read ReadFile(const std::filesystem::path& a_file, bool a_check, const Values& a_before,
		std::string_view a_stands);

	// How many whole letters a_text holds before its first byte that is not
	// UTF-8, in About.cpp.
	[[nodiscard]] std::size_t LettersOf(std::string_view a_text);

	// a_text, a description as SimpleIni gives it, as a file keeps it, in
	// About.cpp.
	[[nodiscard]] std::string AboutWritten(std::string_view a_text);

	// -------------------------------------------------------------------
	// The pick, see Presets.cpp and Actions.cpp
	// -------------------------------------------------------------------

	// What Apply set and read.
	struct Applied
	{
		Changed changed;          // the settings whose value it changed
		Given original;           // what a shipped preset's own file sets
		Given given;              // what the entry's files set, the last one counting
		std::size_t changes = 0;  // how many held settings its changes file sets
		Line start;               // "Preset New Vegas, from ..."
		Lines lines;              // the count line and each file's faults, Default has none
		std::string about;        // the preset's own file's description, empty for Default
	};

	// Sets every held setting by a_entry's stack, rereading its files. A
	// switch left to another mod is left alone.
	[[nodiscard]] Applied Apply(const Entry& a_entry);

	// Everything Presets keeps. The MCM page and the Settings line read it
	// from more than 1 thread, so every read and write holds the lock. With
	// the lock held, never call Settings::ReportLine, which reads it too.
	struct State
	{
		std::mutex lock;
		Entry picked;
		Given original;                 // Applied::original of the pick
		Given given;                    // Applied::given of the pick
		std::string dangling;           // sPreset while the preset it names is gone
		std::vector<Entry> shown;       // the last list handed out
		Lines kept;                     // the game start's lines, for Report
		std::filesystem::path written;  // an own preset NewText wrote again
		std::string about;              // Applied::about of the pick
	};
	[[nodiscard]] State& Now();

	[[nodiscard]] Entry DefaultEntry();

	// An entry's English name as NEC.log and files show it, with " (Custom)"
	// while it has changes.
	[[nodiscard]] std::string ShownOf(const Entry& a_entry);

	[[nodiscard]] bool SameEntry(const Entry& a_lhs, const Entry& a_rhs);

	// Takes a_entry as the pick: the values Apply found and sPreset in
	// NEC_custom.ini. With the lock held.
	void Take(State& a_state, const Entry& a_entry, const Applied& a_applied);

	// The whole text of an own preset from the settings in play: 3 comment
	// lines, then every held setting whose value differs from NEC.ini's, in
	// NEC.ini's sections and order, then a_about as its description when it
	// is not empty. With the lock held, and a_count says how many settings it
	// writes.
	[[nodiscard]] std::string OwnTextHeld(const State& a_state, std::string_view a_name, std::string_view a_from,
		std::string_view a_about, std::size_t& a_count);

	// "1 setting", "3 settings".
	[[nodiscard]] std::string Count(std::size_t a_count, std::string_view a_word);
}
