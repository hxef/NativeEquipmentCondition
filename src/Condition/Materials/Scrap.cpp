#include "Condition/Materials/Materials.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/Materials/Index.h"

#include <algorithm>
#include <format>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <span>

namespace Materials
{
	namespace
	{
		// The latest reading of the lists. Wear reads it under a shared lock,
		// and a new reading replaces it whole under the full lock.
		Recipes           g_own;
		std::shared_mutex g_ownLock;

		// 1 reading at a time. Only a holder of it changes g_own.
		std::mutex g_listsLock;

		// The list a_recipe names when it is a scrap recipe, found the way the
		// bench finds them as its menu opens: a recipe that is not deleted,
		// names a form list and has the keyword the game's default object
		// WorkbenchScrappableKeyword names among its filter keywords,
		// ModRecipeFilterScrap in vanilla. A few recipes each name a list, so
		// every knife scraps into the same 2 steel.
		const RE::BGSListForm* ScrapListOf(RE::BGSConstructibleObject& a_recipe, RE::BGSKeyword& a_scrap)
		{
			auto* list = a_recipe.createdItem ? a_recipe.createdItem->As<RE::BGSListForm>() : nullptr;
			if (!list || a_recipe.IsDeleted() || !a_recipe.filterKeywords.HasKeyword(&a_scrap)) {
				return nullptr;
			}
			return list;
		}

		// Which recipe each weapon and wearable on the lists scraps by now.
		// a_own for the lists' own forms, without what a script added, which
		// the save brings back, so such an add moves no other number.
		// Otherwise every form, as the bench reads them as it scraps. An item
		// on 2 lists scraps by the list a scrap recipe names first in load
		// order. The bench picks by where each list lands in its own index,
		// which agrees for every vanilla item with the usual DLC, and copying
		// that order is not worth it for 1 component on a few items.
		Recipes RecipesOnLists(std::span<const ScrapList> a_lists, bool a_own)
		{
			Recipes out;
			for (const auto& [list, recipe] : a_lists) {
				const auto forms = a_own ? list->GetOwnForms() : std::span{ list->arrayOfForms.data(), list->arrayOfForms.size() };
				for (const auto* form : forms) {
					if (form && (form->Is(RE::ENUM_FORM_ID::kWEAP) || form->Is(RE::ENUM_FORM_ID::kARMO))) {
						out.emplace(form, recipe);
					}
				}
			}
			return out;
		}

		// The items whose scrap recipe differs between 2 readings, for the
		// log. The 5 with the lowest form IDs are named, so the line is the
		// same on every run.
		std::string Changes(const Index& a_index, const Recipes& a_last, const Recipes& a_now)
		{
			constexpr std::size_t NAMED = 5;

			// Each changed item with its new recipe, or nothing when it left
			// every list.
			std::vector<std::pair<const RE::TESForm*, const RE::BGSConstructibleObject*>> changed;
			for (const auto& [form, recipe] : a_now) {
				const auto last = a_last.find(form);
				if (last == a_last.end() || last->second != recipe) {
					changed.emplace_back(form, recipe);
				}
			}
			for (const auto& [form, recipe] : a_last) {
				if (!a_now.contains(form)) {
					changed.emplace_back(form, nullptr);
				}
			}
			std::ranges::sort(changed, {}, [](const auto& a_change) { return a_change.first->formID; });

			std::string out;
			for (const auto& [form, recipe] : changed | std::views::take(NAMED)) {
				const auto name = RE::TESFullName::GetFullName(*form);
				if (recipe) {
					out += std::format(" {:s} [{:08X}] now has the scrap recipe {:s}.", name, form->formID, Spell(a_index, recipe));
				} else {
					out += std::format(" {:s} [{:08X}] no longer has a scrap recipe of its own.", name, form->formID);
				}
			}
			if (changed.size() > NAMED) {
				out += std::format(" {:d} more changed too.", changed.size() - NAMED);
			}
			return out;
		}

		// Makes a_recipes the latest reading. The last one is freed by the
		// caller, after the lock.
		void Publish(Recipes& a_recipes)
		{
			const std::unique_lock l{ g_ownLock };
			g_own.swap(a_recipes);
		}

		// The recipe most items of a kind scrap into, over every item that
		// takes part, has a scrap recipe in a_recipes and a_ofKind keeps. Ties
		// go to the lower form ID, so the answer is the same on every load.
		template <class Item>
		const RE::BGSConstructibleObject* MostBorrowed(const Recipes& a_recipes, bool (*a_ofKind)(const Item&))
		{
			std::unordered_map<const RE::BGSConstructibleObject*, std::uint32_t> items;
			for (const auto* item : g_dataHandler->GetFormArray<Item>()) {
				if (!item || !Condition::WearsOut(*item) || !a_ofKind(*item)) {
					continue;
				}
				const auto own = a_recipes.find(item);
				if (own != a_recipes.end()) {
					items[own->second]++;
				}
			}

			const RE::BGSConstructibleObject* most = nullptr;
			std::uint32_t                     count = 0;
			for (const auto& [recipe, uses] : items) {
				if (uses > count || (uses == count && most && recipe->formID < most->formID)) {
					most = recipe;
					count = uses;
				}
			}
			return most;
		}
	}

	const RE::BGSConstructibleObject* OwnRecipe(const RE::TESForm& a_form)
	{
		const std::shared_lock l{ g_ownLock };
		const auto             own = g_own.find(&a_form);
		return own != g_own.end() ? own->second : nullptr;
	}

	void ReadScrapLists()
	{
		const std::scoped_lock l{ g_listsLock };
		// Nothing measured, and Unload left g_own empty.
		const auto index = Current();
		if (!index) {
			return;
		}
		auto now = RecipesOnLists(index->lists, false);
		if (now == g_own) {
			return;
		}

		REX::INFO("A script or another mod changed a scrap list since NEC last read it, so {:d} items scrap by a recipe of their own, {:d} before.{:s}",
			now.size(), g_own.size(), Changes(*index, g_own, now));
		Publish(now);
	}

	void LoadScrapLists(Index& a_index, bool a_again)
	{
		// The keyword the bench asks for. The game registers the default
		// object under this name, so the lookup finds the one the bench reads.
		auto* store = RE::TESForm::GetFormByEditorID<RE::BGSDefaultObject>("WorkbenchScrappableKeyword");
		auto* scrap = store ? store->GetForm<RE::BGSKeyword>() : nullptr;
		if (!scrap) {
			if (!a_again) {
				REX::WARN("The game's default object WorkbenchScrappableKeyword names no keyword, so NEC finds no scrap recipe and repairs go by an item's mods alone.");
			}
			return;
		}

		for (auto* recipe : g_dataHandler->GetFormArray<RE::BGSConstructibleObject>()) {
			const auto* list = recipe ? ScrapListOf(*recipe, *scrap) : nullptr;
			if (!list) {
				continue;
			}

			// A later recipe naming the same list replaces the earlier one, as
			// in the bench's own index, and the list keeps its place.
			const auto known = std::ranges::find(a_index.lists, list, &ScrapList::list);
			if (known != a_index.lists.end()) {
				known->recipe = recipe;
			} else {
				a_index.lists.push_back({ list, recipe });
			}
		}

		auto& scrapping = a_index.scrapping;
		scrapping.recipes = RecipesOnLists(a_index.lists, true);
		scrapping.borrowedClothing = MostBorrowed<RE::TESObjectARMO>(scrapping.recipes, [](const RE::TESObjectARMO& a_armor) { return ArmorWear::IsClothing(a_armor); });
		scrapping.borrowedArmor = MostBorrowed<RE::TESObjectARMO>(scrapping.recipes, [](const RE::TESObjectARMO& a_armor) { return !ArmorWear::IsClothing(a_armor); });
		scrapping.borrowedWeapon = MostBorrowed<RE::TESObjectWEAP>(scrapping.recipes, [](const RE::TESObjectWEAP&) { return true; });

		// At Load, the first reading of the lists for OwnRecipe, taken
		// without a line in NEC.log.
		if (!a_again) {
			const std::scoped_lock l{ g_listsLock };
			auto                   own = RecipesOnLists(a_index.lists, false);
			Publish(own);
		}
	}

	void UnloadScrapLists()
	{
		const std::scoped_lock l{ g_listsLock };
		Recipes                none;
		Publish(none);
	}
}
