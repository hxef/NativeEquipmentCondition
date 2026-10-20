#pragma once

#include "Core/Plugin.h"

#include <functional>
#include <memory>
#include <string>

// The recipe index Load and MeasureAgain build. Private to this folder.
namespace Materials
{
	// Which recipe builds or scraps each form.
	using Recipes = std::unordered_map<const RE::TESForm*, const RE::BGSConstructibleObject*>;

	// A scrap recipe and the list of items it names.
	struct ScrapList
	{
		const RE::BGSListForm*            list;
		const RE::BGSConstructibleObject* recipe;
	};

	// The scrap recipes as the lists' own forms give them, without what a
	// script added, and what an item with none borrows, see Materials.h.
	struct Scrapping
	{
		Recipes                           recipes;
		const RE::BGSConstructibleObject* borrowedClothing{ nullptr };
		const RE::BGSConstructibleObject* borrowedArmor{ nullptr };
		const RE::BGSConstructibleObject* borrowedWeapon{ nullptr };
	};

	struct Index
	{
		// Which component prices a form, see Components.cpp.
		std::unordered_map<const RE::TESForm*, const RE::BGSComponent*> pricedBy;
		// Which recipe builds each mod. A mod built by 2 recipes keeps the
		// first, as the engine's own search does.
		Recipes mods;
		// The scrap recipes the bench finds, each with its list.
		std::vector<ScrapList> lists;
		Scrapping              scrapping;
		// The 2 medians, see ReferenceQuality in Materials.h.
		float weaponQuality{ 0.0F };
		float armorQuality{ 0.0F };
	};

	// The index readers go by, or nothing before Load and after Unload.
	[[nodiscard]] std::shared_ptr<const Index> Current();

	// The component behind a recipe line, or nothing for a line asking for a
	// finished item priced in caps, which says nothing about what it is made
	// of. A component maps here, and so does a scrap item, to the component
	// LoadComponents gives it.
	[[nodiscard]] const RE::BGSComponent* PricedBy(const Index& a_index, const RE::TESForm* a_form);

	// Whether a recipe asks for any component. A line for a finished item
	// or for nothing at all does not count, as in the bill.
	[[nodiscard]] bool AsksForParts(const Index& a_index, const RE::BGSConstructibleObject& a_recipe);

	// What a recipe asks for, for the log: "2 Cloth", or "nothing" for no
	// recipe at all.
	[[nodiscard]] std::string Spell(const Index& a_index, const RE::BGSConstructibleObject* a_recipe);

	// Indexes every component and every scrap item for PricedBy. Part of a
	// measure, and a_again leaves the junk item line out of NEC.log.
	void LoadComponents(Index& a_index, bool a_again);

	// The scrap recipe of a_form as Load or ReadScrapLists last read the
	// lists, or nothing. Safe on any thread.
	[[nodiscard]] const RE::BGSConstructibleObject* OwnRecipe(const RE::TESForm& a_form);

	// Finds the scrap recipes the way the bench does and reads their lists'
	// own forms, after LoadComponents. At Load it also takes the first reading
	// of the lists for OwnRecipe. a_again leaves the warning out of NEC.log.
	void LoadScrapLists(Index& a_index, bool a_again);

	// Forgets the last reading of the lists.
	void UnloadScrapLists();

	// The recipes an item is built from: its own scrap recipe, or the borrowed
	// one for a wearable without, then the recipe behind every enabled mod, the
	// same 2 in the same order as the engine's scrapping code. Mods no recipe
	// builds are skipped, and so is the 1 legendary effect the game names for
	// the item, even where a plugin gives it a recipe. Last, the borrowed one
	// for a weapon whose recipes asked for no component. a_extra can be null.
	void ForEachRecipe(const Index& a_index, const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra,
		const std::function<void(const RE::BGSConstructibleObject&)>& a_each);
}
