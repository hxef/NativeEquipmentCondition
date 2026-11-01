#include "Gameplay/FoodSpoil/Kinds.h"

#include "Core/IniText.h"
#include "Core/ListFiles.h"
#include "Gameplay/FoodSpoil/Food.h"

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
//
// NEC.food.log lists every food in that form with the kind it ended up with,
// so a player can copy a line into a file there and change the kind.
namespace FoodSpoil
{
	namespace
	{
		// -------------------------------------------------------------------
		// The words
		// -------------------------------------------------------------------

		void Say(const std::string& a_line)
		{
			Note(LogPart::kFoodFiles, a_line);
		}

		constexpr ListFiles::List LIST{ "Food", "keeps every food's kind as the game data gives it", "kind", Say };

		struct Word
		{
			std::string_view text;
			Kind             kind;
		};

		// In the order NEC.food.log counts them.
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

		std::string_view WordOf(Kind a_kind)
		{
			return std::ranges::find(WORDS, a_kind, &Word::kind)->text;
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

		// "Food\MyFood.ini sets 2 foods: 1 raw and 1 that never spoils."
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
			NoteFile(LogPart::kFoodFiles, std::format("{:s} sets {:d} {:s}{:s}.", a_file, total, total == 1 ? "food" : "foods", text));
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
			std::unordered_map<RE::TESFormID, Sorted>& kinds;
			std::unordered_map<RE::TESFormID, First>   firsts{};
			std::array<std::size_t, std::size(WORDS)>  counts{};
		};

		// 1 line into a_reading, said in NEC.food.log.
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
				Say(std::format("{:s} line {:d}: {:s} [{:08X}] is not food, so NEC skips it.", a_line.file, a_line.number, name, id));
				return;
			}
			const auto [first, added] = a_reading.firsts.try_emplace(id, std::string{ a_line.file }, a_line.number);
			if (!added) {
				const auto& kept = first->second;
				const auto  where = kept.file == a_line.file ? std::format("line {:d}", kept.line) :
				                                               std::format("{:s} line {:d}", kept.file, kept.line);
				Say(std::format("{:s} line {:d}: {:s} [{:08X}] is listed again, so NEC keeps {:s}.", a_line.file, a_line.number, name, id, where));
				return;
			}
			food->second = { word->kind, Why::kList, std::string{ a_line.file } };
			a_reading.counts[static_cast<std::size_t>(word - std::begin(WORDS))]++;
		}

		// -------------------------------------------------------------------
		// NEC.food.log's foods
		// -------------------------------------------------------------------

		constexpr std::string_view FOODS_HEAD[]{
			"; Every food and its kind, as a food list line. Copy a line into a file in",
			"; Data\\F4SE\\Plugins\\NEC\\Food to change its kind.",
		};

		// A kind's word with the spaces after it, so the comments line up:
		// never spoils and 3 spaces.
		constexpr std::size_t WORD_WIDTH = 15;

		// Why a_food has its kind, in the words of the rules in Kinds.h.
		std::string WhyOf(const Sorted& a_food)
		{
			switch (a_food.why) {
				case Why::kDrink:
					return "a drink";
				case Why::kCooked:
					return "made at a cooking station";
				case Why::kHarvested:
					return "harvested from a plant";
				case Why::kFruitOrVegetable:
					return "fruit or vegetable";
				case Why::kDiseaseRisk:
					return "a disease risk";
				case Why::kList:
					return "set by " + a_food.list;
				default:
					return "none of the above";
			}
		}

		// The plugin of a_plugins that adds the record a_id names, by its
		// load order slot.
		const RE::TESFile* PluginOf(RE::TESFormID a_id, const std::vector<const RE::TESFile*>& a_plugins)
		{
			const auto at = std::ranges::find_if(a_plugins, [a_id](const RE::TESFile* a_file) { return a_file->GetLoadOrderFormID(a_id) == a_id; });
			return at != a_plugins.end() ? *at : nullptr;
		}

		// "MyFoodMod.esp|000801 = raw            ; Radstag Jerky, a disease
		// risk", or nothing for a form no plugin adds.
		std::string LineOf(RE::TESFormID a_id, const Sorted& a_food, const std::vector<const RE::TESFile*>& a_plugins)
		{
			const auto* plugin = PluginOf(a_id, a_plugins);
			const auto* form = RE::TESForm::GetFormByID(a_id);
			if (!plugin || !form) {
				return {};
			}
			const auto local = plugin->IsLight() ? a_id & 0xFFF : a_id & 0xFFFFFF;
			const auto name = RE::TESFullName::GetFullName(*form);
			const auto why = WhyOf(a_food);
			return std::format("{:s}|{:06X} = {:<{}s}; {:s}", plugin->GetFilename(), local, WordOf(a_food.kind), WORD_WIDTH,
				name.empty() ? why : std::format("{:s}, {:s}", name, why));
		}
	}

	void ReadFoodList(std::unordered_map<RE::TESFormID, Sorted>& a_kinds)
	{
		Reading reading{ a_kinds };
		ListFiles::Read(LIST, [&reading](const ListFiles::Line& a_line, ListFiles::Faults& a_faults) { Take(a_line, a_faults, reading); },
			[&reading](std::string_view a_file) {
				Report(a_file, reading.counts);
				reading.counts = {};
			});
	}

	void NoteFoods(const std::unordered_map<RE::TESFormID, Sorted>& a_kinds)
	{
		std::vector<const RE::TESFile*> plugins;
		for (const auto* plugin : g_dataHandler->files) {
			if (plugin && plugin->IsActive()) {
				plugins.push_back(plugin);
			}
		}
		std::vector<RE::TESFormID> ids;
		for (const auto& [id, food] : a_kinds) {
			ids.push_back(id);
		}
		std::ranges::sort(ids);

		for (const auto line : FOODS_HEAD) {
			Note(LogPart::kFoods, line);
		}
		for (const auto id : ids) {
			if (const auto line = LineOf(id, a_kinds.at(id), plugins); !line.empty()) {
				Note(LogPart::kFoods, line);
			}
		}
	}
}
