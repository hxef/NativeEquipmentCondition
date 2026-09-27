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
// shows for a worn weapon, piece of armor or piece of clothing on the player's
// side at a trader who repairs that kind, see Stock.h, greyed where the trader
// cannot go further, a trade is pending or the player cannot pay for the
// smallest step. A greyed button ignores clicks, so it only responds to the key
// and the pad, and says in the corner why.
//
// Pressing an active button asks how far to repair the item with the price on
// each button, and picking one pays, see Payment.h.
namespace VendorRepair
{
	// The key the button answers, the way the barter screen names it.
	inline constexpr std::string_view HINT_EVENT = "LShoulder"sv;

	// Puts the button on the bar the first time and says whether it shows and
	// whether it can be pressed. Called on every change of highlight and
	// rebuild of a list.
	void Refresh(RE::BarterMenu* a_menu);

	// The button pressed, by mouse or by key. False where there is nothing to
	// press, which leaves the key to the game.
	bool Press();

	// Releases the screen once the question is answered or cancelled: its lists
	// respond again and its bar comes back.
	void Release();

	// Forgets the question, for a new screen, which the game can build on the
	// last one's movie, button and all.
	void ForgetButton();

	// Whether a_menu has the button. A screen that refused one never does, and
	// there the key is the game's.
	[[nodiscard]] bool Offered(RE::BarterMenu* a_menu);
}
