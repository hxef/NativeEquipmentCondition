#pragma once

#include "Core/Plugin.h"

#include "Condition/Materials/Materials.h"
#include "UI/Repair/Workbench/Bench.h"
#include "UI/Repair/Workbench/Cost.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Repairing with a spare, another copy of the same item the player carries,
// taken apart for the repair. Private to this folder.
//
// A spare is worth what its parts are worth, the parts it scraps into without
// the halving, its condition and Scrapper counted, see Materials::Yield, times
// fSpareWorthMult. At 0.5 that is exactly what scrapping it gives. That
// worth buys condition at the item's own repair price, on the curve in
// Repair.h before any rounding to whole parts, so no perk rank, better spare
// or Scrapper rank ever lowers what a spare restores. The price is paid from
// the spare's parts, the cheapest first, and the rest goes to the player,
// never more than the spare holds at 1. With bSpareRepair off no box asks,
// see CarriesSpare.
// SpareBox.h asks which spare to use.
namespace Workbench
{
	// The least a spare restores, even one that scraps into nothing.
	inline constexpr std::uint32_t LEAST_GAIN = 2;

	// The most spares 1 box lists. The box answers with 1 byte, and CANCEL
	// takes the last number.
	inline constexpr std::size_t MOST_SPARES = 254;

	// 1 copy of the item that a repair can use up.
	struct Spare
	{
		// Its stack's list, by which it is found again, since a stack's
		// number moves when a stack before it goes. Compared, never read.
		const RE::ExtraDataList* extra{ nullptr };
		std::string              name;
		std::uint32_t            percent{ 0 };
		bool                     equipped{ false };
		bool                     favorite{ false };
		bool                     legendary{ false };
		// A copy in the item's own stack, the same as the item in every way.
		bool                         withItem{ false };
		// What it is worth at fSpareWorthMult, which pays for the repair.
		std::vector<Materials::Part> scrap;
		// The parts it holds, its worth at 1, from which the leftover comes.
		// The same as scrap at 1 and below.
		std::vector<Materials::Part> held;
		std::uint32_t                worth{ 0 };
		std::uint32_t                gain{ 0 };
	};

	// The player's copy of a_object on the stack with a_extra, read the way
	// the bench reads a row, or nothing once it is gone.
	[[nodiscard]] std::optional<Selection> Held(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra);

	// Whether the player carries a spare for a_item, the bench's item, and
	// bSpareRepair is on.
	[[nodiscard]] bool CarriesSpare(const Selection& a_item);

	// Every spare for a_item, 1 row per copy, the smallest gain first, then
	// the lower condition, then the name, MOST_SPARES at most. Another copy
	// in the player's inventory with the same base counts, equipped,
	// favorite and legendary ones too. Never a quest item, and gear the SCRAP
	// button refuses only as iUnscrappableSpares allows, by default a
	// legendary but no unique weapon. The bench never lists a power armor
	// piece, so neither the item nor a spare is one. Says each in the trace.
	[[nodiscard]] std::vector<Spare> Spares(const Selection& a_item, const Priced& a_priced);

	// The spare on the stack with a_extra, worked out again for a_item as it
	// is now, or nothing once no such copy is left.
	[[nodiscard]] std::optional<Spare> SpareOn(const Selection& a_item, const Priced& a_priced, const RE::ExtraDataList* a_extra);

	// What a spare's repair takes from its parts, and what goes back.
	struct Payment
	{
		std::vector<Materials::Part> taken;
		std::vector<Materials::Part> back;
	};

	// The price of a_spare's gain on a_item at a_priced, paid from its parts
	// the cheapest first. A part is taken while at least half its worth is
	// still owed, and 1 part at least whenever the spare has any, as every
	// repair takes 1. What goes back is what the spare holds less what was
	// taken.
	[[nodiscard]] Payment Pay(const Spare& a_spare, const Selection& a_item, const Priced& a_priced);

	// The spare's row in the box, "(favorite) Hunting Rifle (60%)  +7%".
	[[nodiscard]] std::string RowOf(const Spare& a_spare);
}
