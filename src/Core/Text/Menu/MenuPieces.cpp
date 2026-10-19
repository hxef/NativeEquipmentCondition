#include "Core/Text/Text.h"

#include "Core/Pieces.h"
#include "Core/Text/Lines.h"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The words for the pieces of a part another mod took, see Core/Pieces.h, on
// the MCM page and in NEC.log: a part with its pieces in brackets, what still
// works, and the sentence of a setting with pieces off. Pieces are listed with
// commas only, since some names hold "and".
namespace Text
{
	namespace
	{
		// The most pieces the page names after "Still works", past which it
		// says the rest still works. NEC.log names every one.
		constexpr std::size_t MOST_NAMED = 3;

		std::optional<Key> WordsIn(std::span<const PieceWords> a_rows, Piece a_piece)
		{
			const auto it = std::ranges::find(a_rows, a_piece, &PieceWords::piece);
			return it != a_rows.end() ? std::optional{ it->words } : std::nullopt;
		}

		// A piece by itself: its name, its part's for the only piece of a
		// part, or the phrase of a hidden one.
		std::string PieceName(Piece a_piece, Out a_out)
		{
			const auto& row = RowOfPiece(a_piece);
			if (row.shown == Shown::kPart) {
				return a_out == Out::kLog ? std::string{ PartLogName(row.part) } : PartName(row.part);
			}
			for (const auto rows : { PlayPieces(), ArmorPieces(), UiPieces(), PiecePhrases() }) {
				if (const auto words = WordsIn(rows, a_piece)) {
					return PickFor(*words, a_out);
				}
			}
			return {};
		}

		// A setting's pieces for its sentence: a piece's phrase where it has
		// one, a part's name where all of it is there, the plain names of the
		// setting's first part, and "part (pieces)" for any other part.
		std::vector<std::string> SettingNames(std::span<const Piece> a_pieces, Part a_first, Out a_out)
		{
			std::vector<std::string> names;
			std::vector<Part>        named;
			for (const auto piece : a_pieces) {
				if (const auto phrase = WordsIn(PiecePhrases(), piece)) {
					names.emplace_back(PickFor(*phrase, a_out));
					continue;
				}
				const auto part = RowOfPiece(piece).part;
				if (std::ranges::find(named, part) != named.end()) {
					continue;
				}
				named.push_back(part);
				std::vector<Piece> group;
				for (const auto other : a_pieces) {
					if (RowOfPiece(other).part == part && !WordsIn(PiecePhrases(), other)) {
						group.push_back(other);
					}
				}
				if (part == a_first && !Whole(part, group)) {
					for (const auto member : group) {
						names.push_back(PieceName(member, a_out));
					}
				} else {
					names.push_back(PartPieces(part, group, a_out));
				}
			}
			return names;
		}

		bool IsSwitch(const Settings::Named& a_setting)
		{
			return std::ranges::any_of(Settings::Switches(),
				[&](const auto* a_on) { return static_cast<const Settings::Named*>(a_on) == &a_setting; });
		}
	}

	std::string Sentences(std::string_view a_first, std::string_view a_second, Out a_out)
	{
		return SayFor(a_out, SENTENCES, a_first, a_second);
	}

	std::string StillWorks(std::span<const Piece> a_works, Out a_out)
	{
		if (a_out == Out::kPage && a_works.size() > MOST_NAMED) {
			return PickFor(REST_WORKS, a_out);
		}
		return SayFor(a_out, STILL_WORKS, PieceNames(a_works, a_out));
	}

	std::string PieceNames(std::span<const Piece> a_pieces, Out a_out)
	{
		std::vector<std::string> names;
		for (const auto piece : a_pieces) {
			names.push_back(PieceName(piece, a_out));
		}
		return Commas(names, a_out);
	}

	std::string PartPieces(Part a_part, std::span<const Piece> a_pieces, Out a_out)
	{
		const auto name = a_out == Out::kLog ? std::string{ PartLogName(a_part) } : PartName(a_part);
		if (a_pieces.empty() || Whole(a_part, a_pieces)) {
			return name;
		}
		return SayFor(a_out, PART_PIECES, name, PieceNames(a_pieces, a_out));
	}

	std::vector<std::string> SettingLines(const SettingLink& a_link, const CallPatch::Effect& a_effect, Out a_out,
		std::span<const Piece> a_named)
	{
		std::vector<std::string> lines;
		if (a_effect.idle) {
			return lines;
		}
		// Worn item prices off changes what a repair is priced from, which
		// gets a sentence of its own.
		std::vector<Piece> lost;
		bool               barter = false;
		for (const auto piece : a_effect.off) {
			if (piece == Piece::kPrices && std::ranges::find(a_link.side, piece) != a_link.side.end()) {
				barter = true;
			} else if (std::ranges::find(a_named, piece) == a_named.end()) {
				lost.push_back(piece);
			}
		}
		if (!lost.empty() && !a_effect.works.empty()) {
			const auto first = a_link.core.empty() ? Part::kNone : RowOfPiece(a_link.core.front()).part;
			const auto name = a_out == Out::kLog ? SettingLogName(*a_link.setting) : MenuLine(a_link.setting->key);
			const auto some = SayFor(a_out, SETTING_SOME, name, Commas(SettingNames(lost, first, a_out), a_out));
			const auto rest = a_out == Out::kPage && a_effect.works.size() > MOST_NAMED ?
			                      std::string{ PickFor(REST_WORKS, a_out) } :
			                      SayFor(a_out, IsSwitch(*a_link.setting) ? STILL_WORKS_FOR : STILL_CHANGES,
									  Commas(SettingNames(a_effect.works, first, a_out), a_out));
			lines.push_back(Sentences(some, rest, a_out));
		}
		if (barter) {
			lines.emplace_back(PickFor(TRADER_OWN_PRICE, a_out));
		}
		return lines;
	}
}
