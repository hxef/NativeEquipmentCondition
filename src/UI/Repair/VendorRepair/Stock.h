#pragma once

#include "Core/Plugin.h"

#include "Condition/Condition.h"
#include "UI/Repair/SelectedItem.h"

#include <cstddef>
#include <cstdint>
#include <string>

// The item under the highlight, and whether this trader deals in weapons, in
// armor, or in both. Private to this folder.
//
// The game nowhere says what kind of shop a shop is: 71 of the 81 vendor
// factions in Fallout4.esm share one buy and sell list, and the
// VendorItemWeapon keywords are on no record. So the shelves are counted by
// row, 100 rounds counting once and a pistol once: 5 rows of guns with 1 row in
// 6 a gun is the big shop, and 3 rows of guns and ammunition with half the
// shelves in them is the small stand. Grenades and mines are not guns here.
// Tested against every vendor chest in the game and its 6 add ons, 20 of the 26
// weapon dealers pass either test and no general store, bar, clinic, chem
// dealer or farm stand does. What is counted is the list as it stands, stock
// plus everything the player ever sold them.
//
// Armor is counted the same way on rows of its own, with power armor pieces
// counting as its filler the way ammunition does for weapons: 5 rows of armor
// with 1 row in 6 a piece is the armorer, and 3 rows of armor and power armor
// with half the shelves in them the small stand. Every wearable wears, a dress
// and a scarf too, so a clothes shop is an armorer, and power armor pieces are
// the one wearable a shop can sell that does not, see ArmorWear.h. A trader can
// pass the tests of both kinds and repair both.
//
// How far a trader repairs a kind follows how many rows of it they stock: the
// first 3 rows reach 30% and every 2 more add another 10%, so 10 rows reach 70%
// and 16 reach full. In vanilla that puts Arturo, KL-E-O, Cricket and Corbett
// at 90% for weapons, and only the 2 gunsmiths of Far Harbor reach full.
namespace VendorRepair
{
	// The 2 tests: MANY_ROWS rows of the trade with one row in MANY_SHARE one
	// of them, or SOME_ROWS rows of the trade and what goes with it, ammunition
	// or power armor, with one row in SOME_SHARE one of those.
	inline constexpr std::size_t MANY_ROWS = 5;
	inline constexpr std::size_t MANY_SHARE = 6;
	inline constexpr std::size_t SOME_ROWS = 3;
	inline constexpr std::size_t SOME_SHARE = 2;

	// The scale: the poorest trader of a kind repairs it to MIN_CEILING and one
	// with FULL_ROWS repairs it to full, in a straight line between, rounded
	// down to a level the question can offer.
	inline constexpr std::uint32_t MIN_CEILING = 30;
	inline constexpr std::size_t   FULL_ROWS = 16;

	// How far a trader with a_rows rows of their trade repairs that kind.
	[[nodiscard]] std::uint32_t Reach(std::size_t a_rows);

	// The item highlighted on the player's side of the screen, with what it is
	// worth.
	struct Selection : SelectedItem::Item
	{
		// What the item is worth as a sound price, without wear or the trader's
		// markup, see ItemValue.h.
		std::uint32_t worth{ 0 };

		// True for an item below full condition that has a price.
		[[nodiscard]] bool Worn() const { return Item::Worn() && worth > 0; }
	};

	// Where the highlight has landed. The movie tells code only in passing,
	// once on every change, and the hook that hears it says so here.
	void Highlight(std::int32_t a_row, bool a_inContainer);

	[[nodiscard]] Selection Selected(RE::BarterMenu* a_menu);

	// How far this trader repairs an item of a_kind, or 0 where that kind is
	// not their trade. Read once per build of their stock, and not before the
	// trader's side has been built.
	[[nodiscard]] std::uint32_t Ceiling(RE::BarterMenu* a_menu, Condition::Kind a_kind);

	// Forgets the highlight and the shelves, for a new screen.
	void Forget();

	// Forgets the shelves alone, for a stock built again.
	void ForgetShelves();

	// Whether the button belongs on the bar: a worn item on the player's side
	// at a trader of its kind. The limit and the player's caps do not matter
	// here, so the button stays, greyed, and pressing it says why.
	[[nodiscard]] bool Shown(const Selection& a_selection, std::uint32_t a_ceiling);

	// The barter screen, or nothing if the player has walked away from it.
	[[nodiscard]] RE::BarterMenu* OpenBarter();

	// What to call this trader in the log. The person rather than the chest,
	// since the chest is nameless furniture in a back room.
	[[nodiscard]] std::string Trader(RE::BarterMenu* a_menu);
}
