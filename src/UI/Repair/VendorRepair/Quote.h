#pragma once

#include "Core/Plugin.h"

#include "Condition/CraftingPerks/CraftingPerks.h"
#include "Condition/Repair.h"
#include "UI/Repair/VendorRepair/Stock.h"

#include <cstdint>
#include <vector>

// What a trader wants for each step of a repair. Private to this folder.
//
// The same price curve as the bench, see Repair.h, but based on what the item
// is worth and always at the price with no perk: a crafting perk is the
// player's own skill and no reason for a trader to charge less, and the bench
// gets cheaper as the game goes on while the trader does not. Worth means the
// sound price, without wear and without the trader's markup: with the wear in,
// a broken item would cost almost nothing and a repair in steps would cost
// more, and the markup changes with which side the game priced last.
namespace VendorRepair
{
	// What a trader charges to repair a broken item, as a multiple of what the
	// item is worth.
	inline constexpr float WRECK_MULTIPLE = CraftingPerks::UNSKILLED_MULTIPLE;

	// That multiple with fTraderPriceMult on it, see Settings.h, so the setting
	// halves or doubles every price.
	[[nodiscard]] float Scaled(float a_multiple);

	// What a trader is paid in, which no barter list shows as a row.
	[[nodiscard]] RE::TESBoundObject* Caps();

	// One condition the item can be brought back to, and the caps wanted.
	using Quote = Repair::Step<std::uint32_t>;

	// Every step worth offering, up to the trader's limit. What is owed now and
	// after are each rounded to whole caps before one is taken from the other,
	// so a repair in steps costs what one in one go costs. Never free, 1 cap at
	// least.
	[[nodiscard]] std::vector<Quote> Quotes(const Selection& a_selection, std::uint32_t a_ceiling);

	// The steps the player can pay for. The price rises with the condition
	// bought, so what is left is the cheap end of the list.
	[[nodiscard]] std::vector<Quote> Afforded(const std::vector<Quote>& a_quotes);
}
