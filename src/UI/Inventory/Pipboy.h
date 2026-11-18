#pragma once

#include "Core/Plugin.h"

#include "Condition/Condition.h"

#include <optional>

// Fading the name of a worn out item in the Pip-Boy's lists, since the game
// refuses to equip one and the Pip-Boy gives no hint until it is tried. The
// game and the menu each build their own object per item, and only the item's
// number is shared, as nodeID. So MarkEntry writes down the numbers of worn
// out items as the game builds them, and the frame listener writes the name of
// any row carrying one in half white, which the Pip-Boy's tint turns into half
// strength Pip-Boy colour. Nothing fades while the Pip-Boy's cards are left to
// another mod. Spoiled food fades the same way. The wear rate of each entry's
// card is kept by the same number, see ItemCard/WearRate.cpp, and the kind
// its CND bar marks, see ItemCard/ConditionBar.cpp.
namespace Pipboy
{
	// Adds the frame listener to the Pip-Boy movie and ignores every other.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);

	// Notes whether the item this Pip-Boy entry stands for has worn out or
	// spoiled, the wear rate its card shows and the kind whose penalty point
	// its CND bar marks, by its number, since a flag of our own would never
	// reach the menu. ItemCard calls this while the game builds the entry for
	// a_stack, worn out or not. Listing a stack again forgets what its last
	// entry had.
	void MarkEntry(const RE::PipboyObject& a_entry, const RE::BGSInventoryItem::Stack* a_stack, bool a_broken, std::optional<float> a_rate,
		std::optional<Condition::Kind> a_kind);

	// The wear rate MarkEntry noted for the entry selected in the Pip-Boy's
	// inventory list, the one its card shows. Nothing on a page with no list,
	// or for an entry with no rate.
	[[nodiscard]] std::optional<float> SelectedWearRate(Scaleform::GFx::Movie& a_movie);

	// The kind MarkEntry noted for that entry the same way. Nothing for food
	// and power armor pieces.
	[[nodiscard]] std::optional<Condition::Kind> SelectedPenaltyKind(Scaleform::GFx::Movie& a_movie);
}
