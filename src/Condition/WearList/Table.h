#pragma once

#include "Condition/WearList/WearList.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// What the wear list read, for Load and Of. Private to this folder.
namespace WearList
{
	// The most a line, or all the lines of 1 copy, multiply wear by.
	inline constexpr float MOST_WEAR = 10.0F;

	// What 1 form's lines set, each number at most once. Only a keyword's
	// line may set both damage and protection, since a weapon or a piece of
	// armor may carry it.
	struct Entry
	{
		std::optional<Set> wear;
		std::optional<Set> damage;
		std::optional<Set> protection;
	};

	// Weapons, armor and mods by form ID, keywords in a list each copy
	// walks. Weapon mods and armor mods share 1 map, since no 2 forms share
	// an ID. files names each Set's file, and load is the read they came
	// from.
	struct Table
	{
		std::unordered_map<RE::TESFormID, Entry>             weapons;
		std::unordered_map<RE::TESFormID, Entry>             armors;
		std::unordered_map<RE::TESFormID, Entry>             mods;
		std::vector<std::pair<const RE::BGSKeyword*, Entry>> keywords;
		std::vector<std::string>                             files;
		std::uint32_t                                        load = 0;
	};

	// The list read last, or nothing before Load, after Unload and while no
	// file set a number. Safe from any thread, see Core/Snapshot.h.
	[[nodiscard]] std::shared_ptr<const Table> Current();

	// "Wear/A_First.ini line 5", or "line 5" when a_after is a line of
	// the same file. The tails pass the list read last, which is the list
	// their lines came from, since a tail is built within 1 call of Of
	// and the list is read again only behind a loading screen. Should a
	// read come between, a line of the read before names its file "?".
	[[nodiscard]] std::string Where(const Table& a_table, const Set& a_set, const Set* a_after);
}
