#pragma once

#include "Core/Plugin.h"

#include "Core/Parts.h"
#include "Core/Pieces.h"
#include "Core/Settings.h"
#include "Core/Text/Text.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// 1 sentence by its key, and picking the player's language. Private to this
// folder and its Menu and Names folders.
namespace Text
{
	// 1 line of the language files, by its place in en.ini.
	struct Key
	{
		std::uint16_t index;
	};

	// A line's key as en.ini names it, and its English.
	struct EnglishLine
	{
		std::string_view name;
		const char*      text;
	};
}

// 1 Key per line of publish/Translations/en.ini, named as there, and ENGLISH,
// every line's English in en.ini's order. The build writes it into the build
// folder from en.ini, see xmake/texts.lua.
#include "Core/Text/Keys.h"

namespace Text
{
	// Full condition, as a whole percent.
	inline constexpr std::uint32_t FULL = 100;

	// The line in the player's language, or the English one. It lasts as long
	// as the game, so a caller may keep it. See Translations.cpp.
	[[nodiscard]] const char* Pick(Key a_key);

	// The line in English, for NEC.log, which never reads a language file.
	[[nodiscard]] constexpr const char* InEnglish(Key a_key) { return ENGLISH[a_key.index].text; }

	// That line with a_args filled into its {0} {1} slots. The arguments are
	// taken by reference because std::make_format_args wants lvalues. A
	// language file's line has the English slots or is not used, so this
	// never throws.
	template <class... T>
	[[nodiscard]] std::string Say(Key a_key, const T&... a_args)
	{
		return std::vformat(Pick(a_key), std::make_format_args(a_args...));
	}

	// One option of the MCM page: the setting it changes, its name and the
	// help line under it. The key is read from the setting when asked, so a
	// table does not depend on which file is set up first. A feature switch
	// has no name of its own, it takes the name of the part it stands for,
	// see PartOf in Core/Feature.h. A name has no comma, since the grey lines
	// list names.
	struct MenuRow
	{
		const Settings::Named* setting;
		std::optional<Key>     name;
		Key                    help;
	};

	// The 8 switches and bTraceLogs, in MenuSwitches.cpp, the 5 wear speeds,
	// in MenuWear.cpp, the 12 other Balance numbers, in MenuNumbers.cpp, and
	// the 4 HUD numbers and sLogLevel, in MenuHudLog.cpp, each in NEC.ini's
	// order.
	[[nodiscard]] std::span<const MenuRow> MenuSwitches();
	[[nodiscard]] std::span<const MenuRow> MenuWear();
	[[nodiscard]] std::span<const MenuRow> MenuNumbers();
	[[nodiscard]] std::span<const MenuRow> MenuHudLog();

	// One part's name, see Core/Parts.h. Each table keeps the enum's order. A
	// name has no comma, since it goes into lists.
	struct PartRow
	{
		Part part;
		Key  name;
	};

	[[nodiscard]] std::span<const PartRow> PlayParts();
	[[nodiscard]] std::span<const PartRow> LootParts();
	[[nodiscard]] std::span<const PartRow> UiParts();

	// Whether a_rows hold 1 row a part, from a_first to a_last in the enum's
	// order, so a part's name is found by its place.
	consteval bool PartsInOrder(std::span<const PartRow> a_rows, Part a_first, Part a_last)
	{
		const auto first = static_cast<std::size_t>(a_first);
		for (std::size_t i = 0; i < a_rows.size(); i++) {
			if (static_cast<std::size_t>(a_rows[i].part) != first + i) {
				return false;
			}
		}
		return a_rows.size() == static_cast<std::size_t>(a_last) - first + 1;
	}

	// One piece's name, or the phrase a setting's sentence uses for it, see
	// Core/Pieces.h. The phrases are in Phrases.cpp. Pieces that share a name
	// share its key. A name stands alone in brackets after its part's name,
	// "Worn weapon damage (energy gun damage)", and in a list after
	// "Still works:", so it has no comma and needs no capital.
	struct PieceWords
	{
		Piece piece;
		Key   words;
	};

	[[nodiscard]] std::span<const PieceWords> PlayPieces();
	[[nodiscard]] std::span<const PieceWords> ArmorPieces();
	[[nodiscard]] std::span<const PieceWords> UiPieces();
	[[nodiscard]] std::span<const PieceWords> PiecePhrases();

	// The line a_out reads: the player's, or the English one, see Out.
	[[nodiscard]] const char* PickFor(Key a_key, Out a_out);

	template <class... T>
	[[nodiscard]] std::string SayFor(Out a_out, Key a_key, const T&... a_args)
	{
		return std::vformat(PickFor(a_key, a_out), std::make_format_args(a_args...));
	}

	// "A, B, C", in MenuNotes.cpp. Then in MenuPieces.cpp "Still works: A.",
	// or on the page "The rest still works." past 3 pieces, and 2 sentences
	// on 1 line.
	[[nodiscard]] std::string Commas(std::span<const std::string> a_items, Out a_out);
	[[nodiscard]] std::string StillWorks(std::span<const Piece> a_works, Out a_out);
	[[nodiscard]] std::string Sentences(std::string_view a_first, std::string_view a_second, Out a_out);
}
