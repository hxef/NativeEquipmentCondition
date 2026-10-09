#pragma once

#include "Core/Plugin.h"
#include "Core/Text/Text.h"

#include "UI/Repair/SelectedItem.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// The item under the highlight, and which of weapons, armor and clothing this
// trader deals in. Private to this folder.
//
// The game nowhere says what kind of shop a shop is: 71 of the 81 vendor
// factions in Fallout4.esm share one buy and sell list, and the
// VendorItemWeapon keywords are on no record. So what a trader restocks with,
// see Restock.h, is counted by row, a different item being a row, 100 rounds
// counting once and a pistol once: 5 rows of guns with 1 row in 6 a gun is the
// big shop, and 3 rows of guns and ammunition with half the shelves in them is
// the small stand. Grenades and mines take no wear, so they are not guns here,
// but a gun shop sells them as it sells rounds and they count with ammunition.
// Tested against every trader in the game and its 6 add ons at level 30, no
// general store, bar, clinic, chem dealer or farm stand passes either test for
// weapons. Buying a trader's shelves bare or selling them a crate of pipe guns
// changes none of it.
//
// Armor and clothing are counted the same way, each on rows of its own, told
// apart by ArmorWear::IsClothing since no keyword on a piece does. Power armor
// pieces, the one wearable a shop can sell that does not wear, count as armor's
// filler the way ammunition does for weapons, and clothing has no filler: the
// game's own armorer's list is about a third clothes and its clothier's list
// has some armor, so counting either as filler for the other would make every
// armorer a clothier and every clothier an armorer. Tested the same way,
// Fallon's Basement, the general stores of Brooks and Acadia and the settlement
// clothing stores repair clothing alone, the settlement armor stores, Arturo,
// Patches, Gage and the Atom Cats armor alone, and Kane, Lucas, the 2 gunsmiths
// of Far Harbor and the Institute both.
//
// How far a trader repairs a kind follows how many rows of it they stock: the
// first 3 rows reach 30% and every 2 more add another 10%, so 10 rows reach 70%
// and 16 reach full. Weapons need more, 18 rows, so 10 rows of guns reach 60%
// and only the best gun shops repair a broken gun to full. In vanilla that
// takes the gunsmiths of Diamond City, Goodneighbor, Nuka-World and Far Harbor,
// Cricket, and Smiling Larry at a level 3 store to full for weapons, Ronnie
// Shaw at one, Eleanor and the Dunmores to 90%, and a level 3 store of the
// player's own to 80%.
//
// Each of the 3 kinds is separate. A trader repairs what they stock, each kind
// as far as its own rows go, so a trader with shelves full of all 3 repairs all
// 3 to full, and the line above the question names the kind its limit is for,
// since the next item may be one the trader repairs further.
namespace VendorRepair
{
	// The 2 tests: MANY_ROWS rows of the trade with one row in MANY_SHARE one
	// of them, or SOME_ROWS rows of the trade and what goes with it, ammunition,
	// grenades and mines for weapons and power armor for armor, with one row in
	// SOME_SHARE one of those.
	inline constexpr std::size_t MANY_ROWS = 5;
	inline constexpr std::size_t MANY_SHARE = 6;
	inline constexpr std::size_t SOME_ROWS = 3;
	inline constexpr std::size_t SOME_SHARE = 2;

	// What a trader can deal in, kept with the sentences that name it.
	// ArmorWear::IsClothing tells a piece of clothing from a piece of armor.
	using Text::Trade;

	// The scale: the poorest trader of a kind repairs it to MIN_CEILING and one
	// with FullRows of it repairs it to full, in a straight line between,
	// rounded down to a level the question can offer.
	inline constexpr std::uint32_t MIN_CEILING = 30;
	inline constexpr std::size_t   FULL_ROWS = 16;
	inline constexpr std::size_t   FULL_GUN_ROWS = 18;

	// How many rows of a_trade it takes to repair that kind to full.
	[[nodiscard]] constexpr std::size_t FullRows(Trade a_trade)
	{
		return a_trade == Trade::kWeapons ? FULL_GUN_ROWS : FULL_ROWS;
	}

	// How far a trader with a_rows rows of a_trade repairs that kind.
	[[nodiscard]] std::uint32_t Reach(Trade a_trade, std::size_t a_rows);

	// What the log calls a trade.
	[[nodiscard]] std::string_view Named(Trade a_trade);

	// Which trade an item belongs to, see ArmorWear::IsClothing.
	[[nodiscard]] Trade TradeOf(const RE::TESBoundObject& a_object);

	// How far a trader repairs each kind, 0 for a kind they do not deal in.
	struct Ceilings
	{
		std::uint32_t weapons{ 0 };
		std::uint32_t armor{ 0 };
		std::uint32_t clothing{ 0 };

		[[nodiscard]] std::uint32_t Of(Trade a_trade) const;
	};

	// What the log says a trader repairs, weapons to 90%, armor to 80% and
	// clothing to 40%.
	[[nodiscard]] std::string Repairs(const Ceilings& a_ceilings);

	// How far a_trader repairs each kind, from what they restock with, see
	// Restock for a_merchant. The counts go to the trace log, with a_now on the
	// end.
	[[nodiscard]] Ceilings CeilingsOf(RE::TESObjectREFR* a_merchant, RE::Actor& a_trader, std::string_view a_now);

	// The item highlighted on the player's side of the screen, with what it is
	// worth.
	struct Selection : SelectedItem::Item
	{
		// What the item is worth as a sound price, without wear or the trader's
		// markup, see ItemValue.h.
		std::uint32_t worth{ 0 };

		// Which trade the item belongs to.
		Trade trade{ Trade::kWeapons };

		// True for an item below full condition that has a price.
		[[nodiscard]] bool Worn() const { return Item::Worn() && worth > 0; }
	};

	// Where the highlight has landed. The movie tells code only in passing,
	// whenever the highlight lands or a list is rebuilt and on every move of
	// the quantity slider, and the hook that hears it says so here.
	void Highlight(std::int32_t a_row, bool a_inContainer);

	[[nodiscard]] Selection Selected(RE::BarterMenu* a_menu);

	// How far this trader repairs a_selection, or 0 for an item at full
	// condition or of a kind they do not deal in. What the trader restocks with
	// is worked out the first time a worn item asks, so building the lists does
	// not need it, and kept until the game builds the trader's side again.
	[[nodiscard]] std::uint32_t Ceiling(RE::BarterMenu* a_menu, const Selection& a_selection);

	// Forgets the highlight and the trader, for a new screen.
	void Forget();

	// Forgets what the trader restocks with, for a trader's side built again:
	// the screen opening, a trade, a sale, an investment or a repair paid for,
	// when a script may have changed a list, a chance or a chest since.
	void ForgetStock();

	// Whether the button belongs on the bar: a worn item on the player's side
	// at a trader who repairs its kind. The limit and the player's caps do not
	// matter here, so the button stays, greyed, and pressing it says why.
	[[nodiscard]] bool Shown(const Selection& a_selection, std::uint32_t a_ceiling);

	// The barter screen, or nothing if the player has walked away from it.
	[[nodiscard]] RE::BarterMenu* OpenBarter();

	// What to call this trader in the log. The person rather than the chest,
	// since the chest is nameless furniture in a back room.
	[[nodiscard]] std::string Trader(RE::BarterMenu* a_menu);
	[[nodiscard]] std::string Trader(RE::TESObjectREFR* a_trader);
}
