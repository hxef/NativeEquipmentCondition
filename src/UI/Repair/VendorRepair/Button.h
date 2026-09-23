#pragma once

#include "Core/Plugin.h"

#include <string_view>

// The REPAIR button on the barter screen's bar, and what pressing it does.
// Private to this folder.
//
// The button is an ordinary BSButtonHintData built in the menu's own movie,
// drawn and laid out like the 11 beside it, added to the list of hints the game
// filled. Its key is one the barter screen leaves alone, the shoulder button on
// the other side from INVEST, C on a keyboard and the left bumper on a pad. It
// shows for a worn weapon on the player's side at a weapon trader, greyed where
// the trader cannot go further or the player cannot pay for the smallest step.
// A greyed button ignores clicks, so it only responds to the key and the pad,
// and says in the corner why.
//
// Pressing an active button asks how far to repair the gun with the price on
// each button, and picking one pays at once: the caps go to the trader's chest,
// the weapon comes back at the condition chosen, and the screen updates itself.
// The Pip-Boy's cards are rebuilt once the screen has closed, since until then
// the game marks up every price.
namespace VendorRepair
{
	// The key the button answers, the way the barter screen names it.
	inline constexpr std::string_view HINT_EVENT = "LShoulder"sv;

	// Puts the button on the bar the first time and says whether it shows and
	// whether it can be pressed. Called on every change of highlight and
	// rebuild of a list.
	void Refresh(RE::BarterMenu* a_menu);

	// The button pressed, by mouse or by key.
	void Press();

	// Whether the question is on the screen, so a second press cannot put a
	// second copy of it behind the first.
	[[nodiscard]] bool Asking();

	// Whether a repair has been paid for since the last time this was asked,
	// which is when the Pip-Boy's cards need a rebuild.
	[[nodiscard]] bool TakeCardsOwed();
}
