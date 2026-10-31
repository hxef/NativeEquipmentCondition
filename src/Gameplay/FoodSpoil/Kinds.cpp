#include "Gameplay/FoodSpoil/Kinds.h"

#include "Core/Settings.h"
#include "Core/Snapshot.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
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

		// Every ALCH a cooking station's recipe makes.
		std::unordered_set<RE::TESFormID> Cooked(const Keywords& a_keywords)
		{
			std::unordered_set<RE::TESFormID> cooked;
			for (const auto* recipe : g_dataHandler->GetFormArray<RE::BGSConstructibleObject>()) {
				const auto* made = recipe ? recipe->createdItem : nullptr;
				if (made && a_keywords.cooking && recipe->benchKeyword == a_keywords.cooking && made->Is(RE::ENUM_FORM_ID::kALCH)) {
					cooked.insert(made->formID);
				}
			}
			return cooked;
		}

		// Adds every ALCH a_form gives, itself or through its leveled lists.
		void AddHarvest(const RE::TESForm* a_form, std::size_t a_depth, std::unordered_set<RE::TESFormID>& a_crops)
		{
			if (!a_form || a_depth > MOST_DEPTH) {
				return;
			}
			if (a_form->Is(RE::ENUM_FORM_ID::kALCH)) {
				a_crops.insert(a_form->formID);
				return;
			}
			if (!a_form->Is(RE::ENUM_FORM_ID::kLVLI)) {
				return;
			}
			const auto& list = static_cast<const RE::TESLevItem&>(*a_form);
			const auto  entries = static_cast<std::size_t>(list.baseListCount);
			for (std::size_t i = 0; list.leveledLists && i < entries; i++) {
				AddHarvest(list.leveledLists[i].form, a_depth + 1, a_crops);
			}
		}

		// Every ALCH a plant gives when harvested.
		std::unordered_set<RE::TESFormID> Harvests()
		{
			std::unordered_set<RE::TESFormID> crops;
			for (const auto* plant : g_dataHandler->GetFormArray<RE::TESFlora>()) {
				if (plant) {
					AddHarvest(plant->produceItem, 0, crops);
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
			Keywords                          keywords;
			std::unordered_set<RE::TESFormID> cooked;
			std::unordered_set<RE::TESFormID> crops;
		};

		// a_food's kind by rules 2 to 6.
		Sorted Rules(const RE::AlchemyItem& a_food, const Sources& a_sources)
		{
			const auto& keywords = a_sources.keywords;
			const auto  has = [&a_food](const RE::BGSKeyword* a_keyword) { return Has(a_food, a_keyword); };
			if (std::ranges::any_of(keywords.drinks, has) || (has(keywords.thirst) && !has(keywords.food))) {
				return { Kind::kNever, Why::kDrink };
			}
			if (a_sources.cooked.contains(a_food.formID)) {
				return { Kind::kCooked, Why::kCooked };
			}
			if (a_sources.crops.contains(a_food.formID)) {
				return { Kind::kCrop, Why::kHarvested };
			}
			if (has(keywords.plant)) {
				return { Kind::kCrop, Why::kFruitOrVegetable };
			}
			return std::ranges::any_of(keywords.risks, has) ? Sorted{ Kind::kRaw, Why::kDiseaseRisk } : Sorted{ Kind::kNever, Why::kRest };
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
		// that never spoil.", and "The list is in NEC.food.log." when a_listed.
		void Report(const std::unordered_map<RE::TESFormID, Sorted>& a_kinds, bool a_listed)
		{
			std::array<std::size_t, 4> counts{};
			for (const auto& [id, food] : a_kinds) {
				counts[static_cast<std::size_t>(food.kind)]++;
			}
			const auto crops = counts[static_cast<std::size_t>(Kind::kCrop)];
			const auto never = counts[static_cast<std::size_t>(Kind::kNever)];
			REX::INFO("Food spoiling sorts {:d} {:s}: {:d} raw, {:d} {:s}, {:d} cooked and {:d} that never {:s}.{:s}", a_kinds.size(),
				a_kinds.size() == 1 ? "food" : "foods", counts[static_cast<std::size_t>(Kind::kRaw)], crops,
				crops == 1 ? "crop" : "crops", counts[static_cast<std::size_t>(Kind::kCooked)], never,
				never == 1 ? "spoils" : "spoil", a_listed ? " The list is in NEC.food.log." : "");
		}

		// -------------------------------------------------------------------
		// The table
		// -------------------------------------------------------------------

		// Every food that spoils, sorted by form ID, read from other threads.
		using Table = std::vector<std::pair<RE::TESFormID, Kind>>;

		Snapshot<Table> g_table;
	}

	void LoadKinds()
	{
		if (!g_dataHandler) {
			return;
		}
		auto kinds = Sort();
		ReadFoodList(kinds);
		Report(kinds, Settings::bFoodSpoils.GetValue() && WriteFoodLog(kinds));

		Table table;
		for (const auto& [id, food] : kinds) {
			if (food.kind != Kind::kNever) {
				table.emplace_back(id, food.kind);
			}
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
		const auto table = g_table.Get();
		if (!table) {
			return Kind::kNever;
		}
		const auto at = std::ranges::lower_bound(*table, a_form.formID, {}, &Table::value_type::first);
		return at != table->end() && at->first == a_form.formID ? at->second : Kind::kNever;
	}
}
