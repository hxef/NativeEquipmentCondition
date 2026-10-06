#pragma once

#include "Core/Plugin.h"

#include <functional>

// The recipe index Load builds, for the files of this folder. Private to it.
namespace Materials
{
	// The component behind a recipe line, or nothing for a line asking for a
	// finished item priced in caps, which says nothing about what it is made
	// of. Both forms of a component map here, the component itself and its
	// scrap item.
	[[nodiscard]] const RE::BGSComponent* PricedBy(const RE::TESForm* a_form);

	// The recipes an item is built from: its own scrap recipe, or the borrowed
	// one for a wearable without, then the recipe behind every enabled mod, the
	// same 2 in the same order as the engine's scrapping code. Mods no recipe
	// builds are skipped, and so is the 1 legendary effect the game names for
	// the item, even where a plugin gives it a recipe. a_extra can be null.
	void ForEachRecipe(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra,
		const std::function<void(const RE::BGSConstructibleObject&)>& a_each);
}
