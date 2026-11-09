#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <optional>

// Paying a trader to repair a worn weapon, piece of armor or piece of clothing.
// A workbench asks for components. A trader who deals in that kind of item asks
// for caps, and more of them, since the player pays for someone else's skill,
// so a player deep in Gun Nut repairs cheaply at a bench and one who never
// picked up a wrench pays a trader.
//
// The REPAIR button sits at the end of the barter screen's bar, beside INVEST,
// while the highlight is on a worn item on the player's side at a trader who
// repairs its kind, weapons, armor or clothing. Pressing it asks how far to
// repair the item, with each step's price on its button, and picking one is all
// it takes. Who deals in what, and how far they repair it, is read from what
// the trader restocks with, so a trader a mod adds lands on the same scale and
// buying the shelves bare changes nothing. Every step is priced from what the
// item owes, see Repair.h, so a trader who stops short of full costs nothing
// extra. A trader's own stock is in better shape the further they repair its
// kind, so the best shops sell nearly new.
//
// Restock.h works out what a trader restocks with, Stock.h reads the item under
// the highlight and what the trader deals in, Quote.h what the work costs,
// Button.h the button, Retry.h the wait for a bar under a UI replacer and
// the watch over it, Payment.h the paying and Upkeep.h the trader's own stock.
namespace VendorRepair
{
	// Patches the barter menu's 2 function tables, as a menu and as an input
	// user, and the restock where weapons and armor spawn worn.
	void Install();

	// Listens for Fallout 4's message boxes, which take REPAIR off the bar
	// while they are up.
	void Load();

	// Whether the barter highlight hook still runs, false once another mod
	// cut its place or while NEC has not seen this screen open. A refresh no
	// hook starts, for a message box or a frame of a wait or a watch, asks it
	// first, since it would show the item that hook last stored.
	[[nodiscard]] bool HighlightLive();

	// How far a_trader repairs a_object's kind, a whole percent, 0 for a kind
	// they do not deal in or an actor with no shop. Worked out from what they
	// restock with at every call, as the barter screen does at its opening.
	[[nodiscard]] std::uint32_t LevelFor(RE::Actor& a_trader, const RE::TESBoundObject& a_object);

	// What a copy of a_item at a_stack is worth as a sound price, without the
	// wear or a trader's markup, see ItemValue.h. Under the inventory lock.
	[[nodiscard]] std::uint32_t SoundWorth(const RE::BGSInventoryItem& a_item, std::uint32_t a_stack);

	// The caps a_trader asks to bring a copy of a_object at a_percent, worth
	// a_worth, to a_to, as the barter screen's question prices it: the first
	// step it offers at a_to or above. Nothing above the trader's limit, at
	// or below a_percent, or for an item with no price.
	[[nodiscard]] std::optional<std::uint32_t> PriceFor(RE::Actor& a_trader, RE::TESBoundObject& a_object, std::uint32_t a_percent,
		std::uint32_t a_worth, std::uint32_t a_to);
}
