#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <optional>
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
	// cards and of the 2 lists that weigh fire rates. Patches the sort's note
	// of each stack's condition while fire rate or accuracy needs it, see
	// TakeSortNote. Runs before PatchAims.
	void PatchRates();

	// While worn guns lose accuracy, patches the accuracy calls of the cards
	// and of the sort, and while they lose range, the range calls of the
	// cards, in Aim.cpp.
	void PatchAims();

	// The condition the sort a container or a trader uses noted for the
	// stack it weighs next, in Rate.cpp. A sort hook takes it once, so a
	// call asked with no note never takes the note of the stack before.
	// Nothing when there is no note.
	[[nodiscard]] std::optional<float> TakeSortNote();

	// Whether NEC writes the sort's note, so the sort hooks get one. In
	// Rate.cpp, known once PatchRates ran.
	[[nodiscard]] bool SortNoted();

	// The condition of the item on the card being built, in Rate.cpp.
	// INVALID_HEALTH when no card is being built.
	[[nodiscard]] float ItemHealth();

	// The condition of the equipped copy of a_weapon that the card being
	// built compares its item with, in Rate.cpp. Only one copy of a weapon
	// can be equipped. INVALID_HEALTH when there is none.
	[[nodiscard]] float ComparedHealth(const RE::TESForm* a_weapon);

	// Adds the render listener that keeps a card's CND row above Damage to
	// a menu movie that draws item cards, and passes over any other movie.
	void WatchCard(Scaleform::GFx::Movie& a_movie, std::string_view a_file);
}
