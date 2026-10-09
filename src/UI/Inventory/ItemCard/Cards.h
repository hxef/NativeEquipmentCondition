#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <string_view>

// What the files of this feature share. Private to this folder.
namespace ItemCard
{
	// The game's own translation key for condition, CND in English, so the row
	// reads right in every language.
	inline constexpr const char* CND_TEXT = "$ItemInfo_CND";

	// The equipped items a card is compared with, each with its stack.
	using CompareItems = RE::BSScrapArray<RE::BSTTuple<const RE::BGSInventoryItem*, std::uint32_t>>;

	// The card the game is building on this thread, set while the game's own
	// builder runs: the condition of the item on it, and the equipped
	// items it is compared with, none for the Pip-Boy.
	struct Building
	{
		float               health;
		const CompareItems* compare;
	};

	inline thread_local const Building* t_building = nullptr;

	// Patches the calls that fill item cards. Returns whether either CND call
	// was patched.
	bool PatchCards();

	// While FireRate slows worn weapons, patches the fire rate calls of the
	// cards and of the 2 lists that weigh fire rates.
	void PatchRates();

	// Adds the render listener that keeps a card's CND row above Damage to
	// a menu movie that draws item cards, and passes over any other movie.
	void WatchCard(Scaleform::GFx::Movie& a_movie, std::string_view a_file);
}
