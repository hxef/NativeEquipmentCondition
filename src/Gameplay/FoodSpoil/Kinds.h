#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <string>
#include <unordered_map>

// How fast each food spoils, sorted from the game data at every data load.
// The rules, first match wins:
//
//   1. Not food: no food flag, or the ObjectTypeChem keyword, as on Jet.
//   2. Never spoils: a drink, by ObjectTypeWater, ObjectTypeDrink,
//      ObjectTypeNukaCola or ObjectTypeAlcohol, or by
//      HC_SustenanceType_QuenchesThirst without ObjectTypeFood. So dirty
//      water stays a drink.
//   3. Cooked: made by a recipe at a cooking station.
//   4. Crop: harvested from a plant, or with the FruitOrVegetable keyword.
//   5. Raw: one of Survival's 2 disease risk keywords for food.
//   6. Never spoils: the rest, such as canned and boxed food.
//
// Keywords are found by editor ID, so no form is named here. A mod's food
// list then sets a kind on top, see FoodList.cpp. Private to this folder.
namespace FoodSpoil
{
	enum class Kind : std::uint8_t
	{
		kNever,
		kRaw,
		kCrop,
		kCooked,
	};

	// Which rule or file gave a food its kind, for NEC.food.log.
	enum class Why : std::uint8_t
	{
		kDrink,
		kCooked,
		kHarvested,
		kFruitOrVegetable,
		kDiseaseRisk,
		kRest,
		kList,
	};

	// A food's kind, why it has it, and the food list file that set it, as
	// NEC.log names it, when one did.
	struct Sorted
	{
		Kind        kind;
		Why         why;
		std::string list{};
	};

	// Sorts every food and logs how many of each kind. Reads the food list,
	// and writes NEC.food.log while food spoiling is on.
	void LoadKinds();

	// Forgets every kind, so no food spoils until LoadKinds runs.
	void UnloadKinds();

	// a_form's kind, kNever for anything that is not food. Safe from any
	// thread, see Core/Snapshot.h.
	[[nodiscard]] Kind KindOf(const RE::TESForm& a_form);

	// Reads the food list into a_kinds, which holds the kind of every food
	// by the rules, and says in NEC.log what each file sets. A form a_kinds
	// lacks is not food. In FoodList.cpp.
	void ReadFoodList(std::unordered_map<RE::TESFormID, Sorted>& a_kinds);

	// Writes every food of a_kinds to NEC.food.log beside NEC.log, 1 food
	// list line each, and returns whether it did. In FoodList.cpp.
	bool WriteFoodLog(const std::unordered_map<RE::TESFormID, Sorted>& a_kinds);
}
