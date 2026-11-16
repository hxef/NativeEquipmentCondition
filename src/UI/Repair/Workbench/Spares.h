#pragma once

#include "Core/Plugin.h"

#include "Condition/Materials/Loose.h"
#include "Condition/Materials/Materials.h"
#include "UI/Repair/Workbench/Bench.h"
#include "UI/Repair/Workbench/Cost.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Repairing with a spare, another of the same weapon or armor the player
// carries, with any mods, taken apart for the repair. Private to this folder.
//
// A spare is worth what its components are worth, the components it scraps
// into without the halving, its condition and Scrapper counted, see
// Materials::Yield, times fSpareWorthMult. At 0.5 that is exactly what
// scrapping it gives. That worth buys condition at the item's own repair
// price, on the curve in Repair.h before any rounding to whole components, so
// no perk rank, better spare or Scrapper rank ever lowers what a spare
// restores. The price is paid from the spare's components, the cheapest
// first, and the rest goes to the player, never more than the spare holds
// at 1. With bSpareRepair off no box asks, see CarriesSpare.
// SpareBox.h asks which spare to use, and SpareMods.h which of its mods
// come back.
namespace Workbench
{
	// The least a spare restores, even one that scraps into nothing.
	inline constexpr std::uint32_t LEAST_GAIN = 2;

	// The most spares 1 box lists. The box answers with 1 byte, and CANCEL
	// takes the last number.
	inline constexpr std::size_t MOST_SPARES = 254;

	// 1 spare a repair can use up.
	struct Spare
	{
		// Its stack's list, by which it is found again, since a stack's
		// number moves when a stack before it goes. Compared, never read.
		const RE::ExtraDataList* extra{ nullptr };
		std::string              name;
		// The whole percent its scrap yields count from, and the number its
		// card shows in tenths, see Condition::CardTenths.
		std::uint32_t            percent{ 0 };
		std::uint32_t            tenths{ 0 };
		bool                     equipped{ false };
		bool                     favorite{ false };
		bool                     legendary{ false };
		// A spare in the item's own stack, the same as the item in every way.
		bool                         withItem{ false };
		// What it is worth at fSpareWorthMult, which pays for the repair.
		std::vector<Materials::Part> scrap;
		// The components it holds, its worth at 1, from which the leftover
		// comes. The same as scrap at 1 and below.
		std::vector<Materials::Part> held;
		std::uint32_t                worth{ 0 };
		// The whole steps it pays for, and what it restores in tenths as the
		// box writes it, see RestoredTenths in SpareMods.h.
		std::uint32_t                gain{ 0 };
		std::uint32_t                restores{ 0 };
		// What it would restore with no Scrapper rank, in tenths too, worked
		// out only for a player who holds one, and restores otherwise.
		std::uint32_t                unranked{ 0 };
		// What it would restore with no rank of the perk pricing the item,
		// worked out the same way.
		std::uint32_t                unpriced{ 0 };
		// Every mod that comes off as a loose mod, the cheapest first by its
		// value, then by name and form ID, as the bench orders mods.
		std::vector<Materials::LooseMod> mods;
		// Those the plan it was worked for keeps, see SpareOn. They go back to
		// the player whole, so scrap and held leave them out.
		std::vector<Materials::LooseMod> kept;
	};

	// The player's copy of a_object on the stack with a_extra, read the way
	// the bench reads a row, or nothing once it is gone.
	[[nodiscard]] std::optional<Selection> Held(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra);

	// Whether the player carries a spare for a_item, the bench's item, and
	// bSpareRepair is on.
	[[nodiscard]] bool CarriesSpare(const Selection& a_item);

	// How many spares for a_item the box of spares would list, MOST_SPARES
	// at most, and 0 while bSpareRepair is off.
	[[nodiscard]] std::uint32_t SparesCarried(const Selection& a_item);

	// Whether another copy of a_item could ever be a spare. False while
	// bSpareRepair is off, and for an item whose record carries the keyword
	// the SCRAP button refuses while iUnscrappableSpares keeps such spares out.
	[[nodiscard]] bool SpareCanExist(const Selection& a_item);

	// Every spare for a_item, 1 row per spare, the least restored first, then
	// the lower condition, then the name, MOST_SPARES at most. Another of the
	// same weapon or armor in the player's inventory counts, with any mods,
	// equipped, favorite and legendary ones too. Never a quest item, and gear
	// the SCRAP button refuses only as iUnscrappableSpares allows, by default a
	// legendary but no unique weapon. The bench never lists a power armor
	// piece, so neither the item nor a spare is one. Says each in the trace.
	[[nodiscard]] std::vector<Spare> Spares(const Selection& a_item, const Priced& a_priced);

	// The spare on the stack with a_extra, worked out again for a_item as it
	// is now, or nothing once no such spare is left. a_kept is a plan: the
	// form IDs of the mods that come back whole, 1 mod each. An ID of no mod
	// that comes off is ignored. With none kept it is the whole spare.
	[[nodiscard]] std::optional<Spare> SpareOn(const Selection& a_item, const Priced& a_priced, const RE::ExtraDataList* a_extra,
		std::span<const std::uint32_t> a_kept = {});

	// What the item's whole bill is worth, which a repair from 0 costs
	// a_priced.multiple times.
	[[nodiscard]] double BillWorth(const Priced& a_priced);

	// The most whole steps a_worth pays for from a_from, the card's number in
	// tenths, never past full, the last 1 short when the card shows a tenth:
	// 40 at most from 60.4%. A spare pays for that, but LEAST_GAIN at least.
	[[nodiscard]] std::uint32_t Covered(const Priced& a_priced, std::uint32_t a_from, std::uint32_t a_worth);

	// What a spare's repair takes from its components, and what goes back.
	struct Payment
	{
		std::vector<Materials::Part> taken;
		std::vector<Materials::Part> back;
	};

	// The price of a_spare's gain on a_item at a_priced, paid from its
	// components the cheapest first. A component is taken while at least half
	// its worth is still owed, and 1 component at least whenever the spare has
	// any, as every repair takes 1. What goes back is what the spare holds
	// less what was taken.
	[[nodiscard]] Payment Pay(const Spare& a_spare, const Selection& a_item, const Priced& a_priced);

	// The spare's first 4 columns in the box: its mark, its name, its own
	// condition and what it restores, "(favorite)", "Hunting Rifle", "(60.4%)"
	// and "+7.2%".
	[[nodiscard]] std::vector<std::string> CellsOf(const Spare& a_spare);

	// Which of those holds the name.
	inline constexpr std::size_t NAME_CELL = 1;
}
