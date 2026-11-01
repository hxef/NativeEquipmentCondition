#pragma once

#include "Core/Plugin.h"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

// The lists a mod ships in a folder under Data/F4SE/Plugins/NEC/, such as
// Exclude, 1 form a line:
//
//   MyMod.esp|0x000801
//
// The plugin is the one that adds the record, and the ID is as FO4Edit shows
// it. Its top 2 digits, the load order slot, never count, and for a light
// plugin only the last 3 do. A list may ask for a word after the ID, as in
// "MyMod.esp|000801 = raw". A line that starts with ; or # is a comment, and a
// comment may follow the ID or the word.
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

	// Reads every .ini file in a_list's folder in name order. a_line gets each
	// line that names a form and says in the list's log what it does with it.
	// After a file's lines, the log gets its faults past the first 10 and its
	// plugins missing from the load order, then a_done gets the file's name
	// for its own summary. Reads nothing while the folder is missing or empty, or
	// before the game data has loaded.
	void Read(const List& a_list, const std::function<void(const Line&, Faults&)>& a_line,
		const std::function<void(std::string_view)>& a_done);
}
