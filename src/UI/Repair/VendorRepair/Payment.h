#pragma once

#include "Core/Plugin.h"

#include "UI/Repair/VendorRepair/Quote.h"

#include <cstdint>
#include <vector>

// Carrying a repair out once the player has chosen how far. Private to this
// folder.
//
// Picking a button pays at once: the caps go to the trader's chest the way a
// trade pays them, the item comes back at the condition chosen, and the screen
// updates itself. The screen lists a copy of what the trader holds and counts
// their caps along the bottom from that copy, so the trader's side is built
// again, as the game does after an investment, its one payment outside a trade.
// A pending purchase points into the copy, so nothing is paid for while any
// trade is pending. The Pip-Boy's cards are rebuilt once the screen has closed,
// since until then the game marks up every price.
namespace VendorRepair
{
	// Whether a trade is pending, a pick on either side not yet accepted.
	[[nodiscard]] bool TradePending(RE::BarterMenu* a_menu);

	// Pays the caps and repairs the item, if the item under the highlight is
	// still a_handle's stack a_stack and no trade is pending, and releases the
	// screen either way, see Release. Everything is checked again rather than
	// remembered, since the answer comes back through F4SE's task queue a
	// moment after the question.
	void Pay(Quote a_quote, std::uint32_t a_handle, std::uint32_t a_stack);

	// The kinds of item repaired since the last time this was asked, as form
	// types, which are the kinds whose Pip-Boy cards need a rebuild. Empty
	// where nothing was paid for.
	[[nodiscard]] std::vector<RE::ENUM_FORM_ID> TakeCardsOwed();
}
