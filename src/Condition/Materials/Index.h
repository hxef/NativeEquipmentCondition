#pragma once

#include "Core/Plugin.h"

#include <functional>
#include <string>

// The recipe index Load builds. Private to this folder.
namespace Materials
{
	// The component behind a recipe line, or nothing for a line asking for a
	// finished item priced in caps, which says nothing about what it is made
	// of. A component maps here, and so does a scrap item, to the component
	// LoadComponents gives it.
	[[nodiscard]] const RE::BGSComponent* PricedBy(const RE::TESForm* a_form);

	// Whether a recipe asks for any component. A line for a finished item
	// or for nothing at all does not count, as in the bill.
	[[nodiscard]] bool AsksForParts(const RE::BGSConstructibleObject& a_recipe);

	// What a recipe asks for, for the log: "2 Cloth", or "nothing" for no
	// recipe at all.
	[[nodiscard]] std::string Spell(const RE::BGSConstructibleObject* a_recipe);

	// Indexes every component and every scrap item for PricedBy and returns
	// how many forms that is. Part of Load.
	[[nodiscard]] std::size_t LoadComponents();

	// Forgets what LoadComponents found.
	void UnloadComponents();

	// The recipes an item is built from: its own scrap recipe, or the borrowed
	// one for a wearable without, then the recipe behind every enabled mod, the
	// same 2 in the same order as the engine's scrapping code. Mods no recipe
	// builds are skipped, and so is the 1 legendary effect the game names for
	// the item, even where a plugin gives it a recipe. Last, the borrowed one
	// for a weapon whose recipes asked for no component. a_extra can be null.
	void ForEachRecipe(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra,
		const std::function<void(const RE::BGSConstructibleObject&)>& a_each);
}
