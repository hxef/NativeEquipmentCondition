#pragma once

#include "Core/Plugin.h"

#include "Core/Parts.h"
#include "Core/Pieces.h"
#include "Core/Settings.h"
#include "Core/Text/Text.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <string_view>

// One sentence in every language, and picking the player's. Private to this
// folder and its Menu and Names folders.
namespace Text
{
	// One sentence in one language. code is what sLanguage says. The first
	// row, English, is the fallback. A table holds the languages the game is
	// sold in, plus Simplified Chinese, which the executable names.
	struct Line
	{
		std::string_view code;
		const char*      text;
	};

	// Full condition, as a whole percent.
	inline constexpr std::uint32_t FULL = 100;

	// The row in the player's language, or the English one.
	[[nodiscard]] const char* Pick(std::span<const Line> a_lines);

	// That row with a_args filled into its {0} {1} slots. The arguments are
	// taken by reference because std::make_format_args wants lvalues.
	template <class... T>
	[[nodiscard]] std::string Say(std::span<const Line> a_lines, const T&... a_args)
	{
		return std::vformat(Pick(a_lines), std::make_format_args(a_args...));
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
		std::span<const Line>  name;
		std::span<const Line>  help;
	};

	// The 8 switches and bTraceLogs, in MenuSwitches.cpp, the 4 wear speeds,
	// in MenuWear.cpp, the 10 other Balance numbers, in MenuNumbers.cpp, and
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
		Part                  part;
		std::span<const Line> name;
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
	// share its table. A name stands alone in brackets after its part's name,
	// "Worn weapon damage (energy gun damage)", and in a list after
	// "Still works:", so it has no comma and needs no capital.
	struct PieceWords
	{
		Piece                 piece;
		std::span<const Line> words;
	};

	[[nodiscard]] std::span<const PieceWords> PlayPieces();
	[[nodiscard]] std::span<const PieceWords> ArmorPieces();
	[[nodiscard]] std::span<const PieceWords> UiPieces();
	[[nodiscard]] std::span<const PieceWords> PiecePhrases();

	// The row a_out reads: the player's, or the English one, see Out.
	[[nodiscard]] const char* PickFor(std::span<const Line> a_lines, Out a_out);

	template <class... T>
	[[nodiscard]] std::string SayFor(Out a_out, std::span<const Line> a_lines, const T&... a_args)
	{
		return std::vformat(PickFor(a_lines, a_out), std::make_format_args(a_args...));
	}

	// "A, B, C", in MenuNotes.cpp. Then in MenuPieces.cpp "Still works: A.",
	// or on the page "The rest still works." past 3 pieces, and 2 sentences
	// on 1 line.
	[[nodiscard]] std::string Commas(std::span<const std::string> a_items, Out a_out);
	[[nodiscard]] std::string StillWorks(std::span<const Piece> a_works, Out a_out);
	[[nodiscard]] std::string Sentences(std::string_view a_first, std::string_view a_second, Out a_out);
}
