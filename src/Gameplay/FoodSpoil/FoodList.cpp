#include "Gameplay/FoodSpoil/Kinds.h"

#include "Core/IniText.h"
#include "Core/ListFiles.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// The food list, for a mod whose food the rules sort wrong. A mod ships a file
// in Data/F4SE/Plugins/NEC/Food/, such as MyFood.ini, with 1 food a line in
// the form Core/ListFiles.h gives and its kind after it:
//
//   MyFoodMod.esp|000801 = raw
//   MyFoodMod.esp|000802 = never spoils
//
// The kinds are raw, crop, cooked and never spoils, and a listed kind
// replaces the rules' pick. NEC ships no file there.
namespace FoodSpoil
{
	namespace
	{
		// -------------------------------------------------------------------
		// The words
		// -------------------------------------------------------------------

		constexpr ListFiles::List LIST{ "Food", "keeps every food's kind as the game data gives it", "kind" };

		struct Word
		{
			std::string_view text;
			Kind             kind;
		};

		// In the order NEC.log counts them.
		constexpr Word WORDS[]{
			{ "raw", Kind::kRaw },
			{ "crop", Kind::kCrop },
			{ "cooked", Kind::kCooked },
			{ "never spoils", Kind::kNever },
		};

		const Word* WordOf(std::string_view a_text)
		{
			const auto at = std::ranges::find_if(WORDS, [a_text](const Word& a_word) { return IniText::SameName(a_word.text, a_text); });
			return at != std::end(WORDS) ? &*at : nullptr;
		}

		// "1 raw", "2 crops", "1 cooked" or "3 that never spoil".
		std::string Counted(Kind a_kind, std::size_t a_count)
		{
			switch (a_kind) {
				case Kind::kRaw:
					return std::format("{:d} raw", a_count);
				case Kind::kCrop:
					return std::format("{:d} {:s}", a_count, a_count == 1 ? "crop" : "crops");
				case Kind::kCooked:
					return std::format("{:d} cooked", a_count);
				default:
					return std::format("{:d} that never {:s}", a_count, a_count == 1 ? "spoils" : "spoil");
			}
		}

		// "Food/MyFood.ini sets 2 foods: 1 raw and 1 that never spoils."
		void Report(std::string_view a_file, const std::array<std::size_t, std::size(WORDS)>& a_counts)
		{
			std::vector<std::string> parts;
			std::size_t              total = 0;
			for (std::size_t i = 0; i < std::size(WORDS); i++) {
				if (a_counts[i] > 0) {
					parts.push_back(Counted(WORDS[i].kind, a_counts[i]));
					total += a_counts[i];
				}
			}
			std::string text;
			for (std::size_t i = 0; i < parts.size(); i++) {
				text += (i == 0 ? ": " : i + 1 == parts.size() ? " and " : ", ") + parts[i];
			}
			REX::INFO("{:s} sets {:d} {:s}{:s}.", a_file, total, total == 1 ? "food" : "foods", text);
		}

		// -------------------------------------------------------------------
		// The lines
		// -------------------------------------------------------------------

		// Where a food was first given a kind.
		struct First
		{
			std::string file;
			std::size_t line;
		};

		// What the files set so far.
		struct Reading
		{
			std::unordered_map<RE::TESFormID, Kind>&  kinds;
			std::unordered_map<RE::TESFormID, First>  firsts{};
			std::array<std::size_t, std::size(WORDS)> counts{};
		};

		// 1 line into a_reading, said in NEC.log.
		void Take(const ListFiles::Line& a_line, ListFiles::Faults& a_faults, Reading& a_reading)
		{
			const auto* word = WordOf(a_line.word);
			if (!word) {
				a_faults.Add(std::format("{:s} line {:d}: {:s} is not raw, crop, cooked or never spoils, so NEC skips it.",
					a_line.file, a_line.number, a_line.word));
				return;
			}
			const auto id = a_line.form.formID;
			const auto name = RE::TESFullName::GetFullName(a_line.form);
			const auto food = a_reading.kinds.find(id);
			if (food == a_reading.kinds.end()) {
				REX::INFO("{:s} line {:d}: {:s} [{:08X}] is not food, so NEC skips it.", a_line.file, a_line.number, name, id);
				return;
			}
			const auto [first, added] = a_reading.firsts.try_emplace(id, std::string{ a_line.file }, a_line.number);
			if (!added) {
				const auto& kept = first->second;
				const auto  where = kept.file == a_line.file ? std::format("line {:d}", kept.line) :
				                                               std::format("{:s} line {:d}", kept.file, kept.line);
				REX::INFO("{:s} line {:d}: {:s} [{:08X}] is listed again, so NEC keeps {:s}.", a_line.file, a_line.number, name, id, where);
				return;
			}
			food->second = word->kind;
			a_reading.counts[static_cast<std::size_t>(word - std::begin(WORDS))]++;
		}
	}

	void ReadFoodList(std::unordered_map<RE::TESFormID, Kind>& a_kinds)
	{
		Reading reading{ a_kinds };
		ListFiles::Read(LIST, [&reading](const ListFiles::Line& a_line, ListFiles::Faults& a_faults) { Take(a_line, a_faults, reading); },
			[&reading](std::string_view a_file) {
				Report(a_file, reading.counts);
				reading.counts = {};
			});
	}
}
