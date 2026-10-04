#include "Core/Text/Text.h"

#include "Core/Pieces.h"
#include "Core/Text/Lines.h"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
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

		// A part with the pieces of it that are off: "CND on item cards
		// (containers and traders)".
		constexpr Line PART_PIECES[]{
			{ "en", "{0} ({1})" },
			{ "fr", "{0} ({1})" },
			{ "de", "{0} ({1})" },
			{ "it", "{0} ({1})" },
			{ "es", "{0} ({1})" },
			{ "esmx", "{0} ({1})" },
			{ "ptbr", "{0} ({1})" },
			{ "pl", "{0} ({1})" },
			{ "ru", "{0} ({1})" },
			{ "ja", "{0}({1})" },
			{ "zhhant", "{0}（{1}）" },
			{ "zhhans", "{0}（{1}）" },
		};

		// After a part's line, what of the part still works.
		constexpr Line STILL_WORKS[]{
			{ "en", "Still works: {0}." },
			{ "fr", "Fonctionne encore\u00A0: {0}." },
			{ "de", "Funktioniert weiterhin: {0}." },
			{ "it", "Funziona ancora: {0}." },
			{ "es", "Sigue funcionando: {0}." },
			{ "esmx", "Sigue funcionando: {0}." },
			{ "ptbr", "Ainda funciona: {0}." },
			{ "pl", "Nadal działa: {0}." },
			{ "ru", "Продолжает работать: {0}." },
			{ "ja", "引き続き有効: {0}" },
			{ "zhhant", "仍可使用：{0}。" },
			{ "zhhans", "仍可使用：{0}。" },
		};

		constexpr Line REST_WORKS[]{
			{ "en", "The rest still works." },
			{ "fr", "Le reste fonctionne encore." },
			{ "de", "Der Rest funktioniert weiterhin." },
			{ "it", "Il resto funziona ancora." },
			{ "es", "El resto sigue funcionando." },
			{ "esmx", "El resto sigue funcionando." },
			{ "ptbr", "O resto ainda funciona." },
			{ "pl", "Reszta nadal działa." },
			{ "ru", "Остальное продолжает работать." },
			{ "ja", "残りは引き続き有効です" },
			{ "zhhant", "其餘仍可使用。" },
			{ "zhhans", "其余仍可使用。" },
		};

		// A setting with some pieces off, then what it still changes, for a
		// number, or still works for, for a switch.
		constexpr Line SETTING_SOME[]{
			{ "en", "{0}: no effect on {1} for now." },
			{ "fr", "{0}\u00A0: sans effet sur {1} pour l'instant." },
			{ "de", "{0}: vorerst keine Wirkung auf {1}." },
			{ "it", "{0}: nessun effetto per {1} per ora." },
			{ "es", "{0}: sin efecto en {1} por ahora." },
			{ "esmx", "{0}: sin efecto en {1} por ahora." },
			{ "ptbr", "{0}: sem efeito para {1} por enquanto." },
			{ "pl", "{0}: na razie bez wpływu na: {1}." },
			{ "ru", "{0}: пока не влияет на: {1}." },
			{ "ja", "{0}: 今のところ{1}には効果がありません" },
			{ "zhhant", "{0}：目前對{1}沒有效果。" },
			{ "zhhans", "{0}：暂时对{1}不生效。" },
		};

		constexpr Line STILL_CHANGES[]{
			{ "en", "Still changes {0}." },
			{ "fr", "Modifie encore {0}." },
			{ "de", "Ändert weiterhin {0}." },
			{ "it", "Cambia ancora {0}." },
			{ "es", "Aún cambia {0}." },
			{ "esmx", "Aún cambia {0}." },
			{ "ptbr", "Ainda altera {0}." },
			{ "pl", "Nadal zmienia: {0}." },
			{ "ru", "По-прежнему меняет: {0}." },
			{ "ja", "{0}には引き続き効果があります" },
			{ "zhhant", "仍會影響{0}。" },
			{ "zhhans", "仍会影响{0}。" },
		};

		constexpr Line STILL_WORKS_FOR[]{
			{ "en", "Still works for {0}." },
			{ "fr", "Fonctionne encore pour {0}." },
			{ "de", "Wirkt weiterhin für {0}." },
			{ "it", "Funziona ancora per {0}." },
			{ "es", "Sigue funcionando para {0}." },
			{ "esmx", "Sigue funcionando para {0}." },
			{ "ptbr", "Ainda funciona para {0}." },
			{ "pl", "Nadal działa dla: {0}." },
			{ "ru", "По-прежнему действует для: {0}." },
			{ "ja", "{0}には引き続き有効です" },
			{ "zhhant", "仍適用於{0}。" },
			{ "zhhans", "仍适用于{0}。" },
		};

		// Trader repairs and Trader repair price while Worn item prices is
		// left to another mod: a quote then starts from the game's own barter
		// price, which can be the buying or the selling side.
		constexpr Line TRADER_OWN_PRICE[]{
			{ "en", "Repair prices follow the item's barter price for now." },
			{ "fr", "Les prix de réparation suivent le prix de troc de l'objet pour l'instant." },
			{ "de", "Reparaturpreise folgen vorerst dem Handelspreis des Gegenstands." },
			{ "it", "Per ora i prezzi di riparazione seguono il prezzo di scambio dell'oggetto." },
			{ "es", "Por ahora los precios de reparación siguen el precio de trueque del objeto." },
			{ "esmx", "Por ahora los precios de reparación siguen el precio de trueque del objeto." },
			{ "ptbr", "Por enquanto os preços de conserto seguem o preço de troca do item." },
			{ "pl", "Ceny napraw na razie opierają się na cenie handlowej przedmiotu." },
			{ "ru", "Цены ремонта пока равны торговой цене предмета." },
			{ "ja", "今のところ、修理価格はアイテム本来の取引価格が基準です" },
			{ "zhhant", "目前由物品原本的交易價格決定修理價格。" },
			{ "zhhans", "暂时由物品原本的交易价格决定修理价格。" },
		};

		// 2 sentences on 1 line, so a language that puts no space between
		// sentences can drop it.
		constexpr Line SENTENCES[]{
			{ "en", "{0} {1}" },
			{ "fr", "{0} {1}" },
			{ "de", "{0} {1}" },
			{ "it", "{0} {1}" },
			{ "es", "{0} {1}" },
			{ "esmx", "{0} {1}" },
			{ "ptbr", "{0} {1}" },
			{ "pl", "{0} {1}" },
			{ "ru", "{0} {1}" },
			{ "ja", "{0}。{1}" },
			{ "zhhant", "{0}{1}" },
			{ "zhhans", "{0}{1}" },
		};

		std::span<const Line> WordsIn(std::span<const PieceWords> a_rows, Piece a_piece)
		{
			const auto it = std::ranges::find(a_rows, a_piece, &PieceWords::piece);
			return it != a_rows.end() ? it->words : std::span<const Line>{};
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
				if (const auto words = WordsIn(rows, a_piece); !words.empty()) {
					return PickFor(words, a_out);
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
				if (const auto phrase = WordsIn(PiecePhrases(), piece); !phrase.empty()) {
					names.emplace_back(PickFor(phrase, a_out));
					continue;
				}
				const auto part = RowOfPiece(piece).part;
				if (std::ranges::find(named, part) != named.end()) {
					continue;
				}
				named.push_back(part);
				std::vector<Piece> group;
				for (const auto other : a_pieces) {
					if (RowOfPiece(other).part == part && WordsIn(PiecePhrases(), other).empty()) {
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
