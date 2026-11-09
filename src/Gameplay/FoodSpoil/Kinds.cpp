#include "Gameplay/FoodSpoil/Kinds.h"

#include "Core/Snapshot.h"
#include "Gameplay/FoodSpoil/Food.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace FoodSpoil
{
	namespace
	{
		// -------------------------------------------------------------------
		// What the rules read
		// -------------------------------------------------------------------

		// The food flag of an ALCH record. 27 chems carry it too.
		constexpr std::uint32_t FOOD_FLAG = 0x2;

		// How deep a plant's leveled lists are walked.
		constexpr std::size_t MOST_DEPTH = 8;

		// The keywords the rules read, any of them missing when a mod
		// removed it.
		struct Keywords
		{
			const RE::BGSKeyword*                chem;
			const RE::BGSKeyword*                food;
			std::array<const RE::BGSKeyword*, 4> drinks;
			const RE::BGSKeyword*                thirst;
			const RE::BGSKeyword*                plant;
			std::array<const RE::BGSKeyword*, 2> risks;
			const RE::BGSKeyword*                cooking;
		};

		const RE::BGSKeyword* KeywordOf(const char* a_editorID)
		{
			return RE::TESForm::GetFormByEditorID<RE::BGSKeyword>(a_editorID);
		}

		Keywords FindKeywords()
		{
			return {
				.chem = KeywordOf("ObjectTypeChem"),
				.food = KeywordOf("ObjectTypeFood"),
				.drinks = { KeywordOf("ObjectTypeWater"), KeywordOf("ObjectTypeDrink"), KeywordOf("ObjectTypeNukaCola"),
					KeywordOf("ObjectTypeAlcohol") },
				.thirst = KeywordOf("HC_SustenanceType_QuenchesThirst"),
				.plant = KeywordOf("FruitOrVegetable"),
				.risks = { KeywordOf("HC_DiseaseRisk_FoodHigh"), KeywordOf("HC_DiseaseRisk_FoodStandard") },
				.cooking = KeywordOf("WorkbenchCooking"),
			};
		}

		bool Has(const RE::AlchemyItem& a_food, const RE::BGSKeyword* a_keyword)
		{
			return a_keyword && a_food.HasKeyword(a_keyword);
		}

		// Each ALCH a recipe or plant gives, and the plugin that last changed
		// the first such record. Records come in load order, so a vanilla food
		// a mod adds a 2nd recipe for keeps the vanilla recipe's plugin.
		using Givers = std::unordered_map<RE::TESFormID, const RE::TESFile*>;

		// Every ALCH a cooking station's recipe makes.
		Givers Cooked(const Keywords& a_keywords)
		{
			Givers cooked;
			for (const auto* recipe : g_dataHandler->GetFormArray<RE::BGSConstructibleObject>()) {
				const auto* made = recipe ? recipe->createdItem : nullptr;
				if (made && a_keywords.cooking && recipe->benchKeyword == a_keywords.cooking && made->Is(RE::ENUM_FORM_ID::kALCH)) {
					cooked.try_emplace(made->formID, recipe->GetFile(-1));
				}
			}
			return cooked;
		}

		// Adds every ALCH a_form gives, itself or through its leveled lists,
		// with a_plant's plugin.
		void AddHarvest(const RE::TESForm* a_form, const RE::TESFile* a_plant, std::size_t a_depth, Givers& a_crops)
		{
			if (!a_form || a_depth > MOST_DEPTH) {
				return;
			}
			if (a_form->Is(RE::ENUM_FORM_ID::kALCH)) {
				a_crops.try_emplace(a_form->formID, a_plant);
				return;
			}
			if (!a_form->Is(RE::ENUM_FORM_ID::kLVLI)) {
				return;
			}
			const auto& list = static_cast<const RE::TESLevItem&>(*a_form);
			const auto  entries = static_cast<std::size_t>(list.baseListCount);
			for (std::size_t i = 0; list.leveledLists && i < entries; i++) {
				AddHarvest(list.leveledLists[i].form, a_plant, a_depth + 1, a_crops);
			}
		}

		// Every ALCH a plant gives when harvested.
		Givers Harvests()
		{
			Givers crops;
			for (const auto* plant : g_dataHandler->GetFormArray<RE::TESFlora>()) {
				if (plant) {
					AddHarvest(plant->produceItem, plant->GetFile(-1), 0, crops);
				}
			}
			return crops;
		}

		// -------------------------------------------------------------------
		// Sorting
		// -------------------------------------------------------------------

		// What rules 3 and 4 read, gathered once for every food.
		struct Sources
		{
			Keywords keywords;
			Givers   cooked;
			Givers   crops;
		};

		// a_food's kind by rules 2 to 6.
		Sorted Rules(const RE::AlchemyItem& a_food, const Sources& a_sources)
		{
			const auto& keywords = a_sources.keywords;
			const auto  has = [&a_food](const RE::BGSKeyword* a_keyword) { return Has(a_food, a_keyword); };
			if (std::ranges::any_of(keywords.drinks, has) || (has(keywords.thirst) && !has(keywords.food))) {
				return { Kind::kNever, Why::kDrink };
			}
			if (const auto cooked = a_sources.cooked.find(a_food.formID); cooked != a_sources.cooked.end()) {
				return { Kind::kCooked, Why::kCooked, {}, cooked->second };
			}
			if (const auto crop = a_sources.crops.find(a_food.formID); crop != a_sources.crops.end()) {
				return { Kind::kCrop, Why::kHarvested, {}, crop->second };
			}
			if (has(keywords.plant)) {
				return { Kind::kCrop, Why::kFruitOrVegetable, {}, a_food.GetFile(-1) };
			}
			if (std::ranges::any_of(keywords.risks, has)) {
				return { Kind::kRaw, Why::kDiseaseRisk, {}, a_food.GetFile(-1) };
			}
			return { Kind::kNever, Why::kRest };
		}

		// The kind of every food.
		std::unordered_map<RE::TESFormID, Sorted> Sort()
		{
			const auto    keywords = FindKeywords();
			const Sources sources{ keywords, Cooked(keywords), Harvests() };

			std::unordered_map<RE::TESFormID, Sorted> kinds;
			for (const auto* food : g_dataHandler->GetFormArray<RE::AlchemyItem>()) {
				if (food && (food->data.flags & FOOD_FLAG) != 0 && !Has(*food, sources.keywords.chem)) {
					kinds.emplace(food->formID, Rules(*food, sources));
				}
			}
			return kinds;
		}

		// "Food spoiling sorts 280 foods: 42 raw, 39 crops, 47 cooked and 152
		// that never spoil."
		void Report(const std::unordered_map<RE::TESFormID, Sorted>& a_kinds)
		{
			std::array<std::size_t, 4> counts{};
			for (const auto& [id, food] : a_kinds) {
				counts[static_cast<std::size_t>(food.kind)]++;
			}
			const auto crops = counts[static_cast<std::size_t>(Kind::kCrop)];
			const auto never = counts[static_cast<std::size_t>(Kind::kNever)];
			Note(LogPart::kFoods, std::format("Food spoiling sorts {:d} {:s}: {:d} raw, {:d} {:s}, {:d} cooked and {:d} that never {:s}.",
				a_kinds.size(), a_kinds.size() == 1 ? "food" : "foods", counts[static_cast<std::size_t>(Kind::kRaw)], crops,
				crops == 1 ? "crop" : "crops", counts[static_cast<std::size_t>(Kind::kCooked)], never, never == 1 ? "spoils" : "spoil"));
		}

		// -------------------------------------------------------------------
		// The table
		// -------------------------------------------------------------------

		// Every food, sorted by form ID, read from other threads. Food that
		// never spoils is kept too, so a script can tell it from what is not
		// food.
		using Table = std::vector<std::pair<RE::TESFormID, Kind>>;

		Snapshot<Table> g_table;

		std::optional<Kind> Find(const RE::TESForm& a_form)
		{
			const auto table = g_table.Get();
			if (!table) {
				return std::nullopt;
			}
			const auto at = std::ranges::lower_bound(*table, a_form.formID, {}, &Table::value_type::first);
			return at != table->end() && at->first == a_form.formID ? std::optional{ at->second } : std::nullopt;
		}
	}

	void LoadKinds()
	{
		if (!g_dataHandler) {
			return;
		}
		auto kinds = Sort();
		ReadFoodList(kinds);
		Report(kinds);
		NoteFoods(kinds);

		Table table;
		table.reserve(kinds.size());
		for (const auto& [id, food] : kinds) {
			table.emplace_back(id, food.kind);
		}
		std::ranges::sort(table);
		g_table.Publish(std::make_shared<const Table>(std::move(table)));
	}

	void UnloadKinds()
	{
		g_table.Publish(nullptr);
	}

	Kind KindOf(const RE::TESForm& a_form)
	{
		return Find(a_form).value_or(Kind::kNever);
	}

	std::int32_t KindNumber(const RE::TESForm& a_form)
	{
		const auto kind = Find(a_form);
		return kind ? static_cast<std::int32_t>(*kind) : -1;
	}
}
