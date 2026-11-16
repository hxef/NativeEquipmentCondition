#pragma once

#include "Core/Plugin.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// The lists a mod ships in a folder under Data/F4SE/Plugins/NEC/, such as
// Exclude, 1 form a line:
//
//   MyMod.esp|0x000801
//
// The plugin is the one that adds the record, and the ID is as FO4Edit shows
// it. For a full plugin the first 2 digits of an ID, the load order slot,
// never count. For a light plugin (.esl or ESL flagged), write the last 3
// digits, such as 801, or the whole ID, such as FE001801. A light plugin's
// ID that starts with FE counts only its last 3 digits. Any other counts its
// last 6, which work only up to FFF, so NEC skips 001801. A list may ask for
// a word after the ID, as in "MyMod.esp|000801 = raw". A line that starts
// with ; or # is a comment, and a comment may follow the ID or the word.
namespace ListFiles
{
	// What sets 1 list apart.
	struct List
	{
		// The folder's name, in plain ASCII. NEC.log names a file as
		// "Exclude/MyMod.ini".
		std::string_view folder;

		// What NEC does when the folder cannot be read, said after "so NEC",
		// such as "leaves no item alone".
		std::string_view unread;

		// The word each line ends in after "=", such as "kind", or empty for
		// a list whose lines end at the ID.
		std::string_view word;

		// Where the list's lines go when it keeps a log of its own, as the
		// food and fridge lists keep NEC.food.log. Such a log names a file
		// in Windows' spelling, "Food\MyMod.ini". Empty for NEC.log.
		void (*say)(const std::string& a_line) = nullptr;

		// What the log adds after it skips an ID the game made, which lasts
		// 1 save, such as what to name instead. Empty for nothing.
		std::string_view madeHint{};
	};

	// A file's warnings. The first 10 go to the list's log and the rest are
	// only counted.
	class Faults
	{
	public:
		explicit Faults(const List& a_list) :
			_list(a_list)
		{}

		void Add(const std::string& a_line);

		[[nodiscard]] std::size_t Count() const { return _count; }

	private:
		const List& _list;
		std::size_t _count = 0;
	};

	// 1 line whose form is in the load order.
	struct Line
	{
		std::string_view file;    // as the list's log names it
		std::size_t      number;  // the first line is 1
		RE::TESForm&     form;
		std::string_view word;    // trimmed, empty for a list with no word
	};

	// 1 line whose plugin is in the load order but whose form is not loaded,
	// as a reference the game loads only with its cell.
	struct Unloaded
	{
		std::string_view   file;    // as the list's log names it
		std::size_t        number;  // the first line is 1
		const RE::TESFile& plugin;
		std::uint32_t      local;  // its ID in the plugin, see TESFile.h
		std::string_view   word;   // trimmed, empty for a list with no word
	};

	// Reads every .ini file in a_list's folder in name order. a_line gets each
	// line that names a form and says in the list's log what it does with it.
	// An 8 digit ID that starts with FF is a form the game made, so the log
	// skips its line before it looks for the plugin. After a file's lines, the
	// log gets its faults past the first 10 and its plugins missing from the
	// load order, then a_done gets the file's name for its own summary. Reads
	// nothing while the folder is missing or empty, or before the game data
	// has loaded.
	void Read(const List& a_list, const std::function<void(const Line&, Faults&)>& a_line,
		const std::function<void(std::string_view)>& a_done);

	// The same, but a line whose form is not loaded goes to a_unloaded
	// instead of the log's "has no form".
	void Read(const List& a_list, const std::function<void(const Line&, Faults&)>& a_line,
		const std::function<void(const Unloaded&, Faults&)>& a_unloaded, const std::function<void(std::string_view)>& a_done);

	// Words a list file's summary and lines share.

	// ": a, b and c" to end a summary's count, ": a" for 1 part, empty for none.
	[[nodiscard]] std::string Joined(const std::vector<std::string>& a_parts);

	// A form the game made, whose ID starts with FF and lasts 1 save. This is
	// for a bare ID. A loaded form asks its own IsCreated().
	[[nodiscard]] bool Made(RE::TESFormID a_id);

	// The plugin in the load order that adds the record a_id names, by the
	// slot a_id carries. nullptr for a form the game made, or before the game
	// data has loaded.
	[[nodiscard]] const RE::TESFile* PluginOf(RE::TESFormID a_id);

	// a_id inside a_plugin, its last 3 digits for a light plugin, 6 for any other.
	[[nodiscard]] std::uint32_t LocalOf(const RE::TESFile& a_plugin, RE::TESFormID a_id);

	// "Fallout4.esm|002CB9", as a line names a form.
	[[nodiscard]] std::string Spelled(const RE::TESFile& a_plugin, std::uint32_t a_local);
}
