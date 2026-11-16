#pragma once

#include "Core/Plugin.h"

#include "Condition/Materials/Materials.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

// Repairing a worn weapon or piece of armor at its workbench. An item below
// the mod lock, full unless iModdingFrom says less, cannot be modified: its
// row is faded, it gets no mod slots, the heading over them goes, and asking
// for them says why. Instead a REPAIR button on the bar asks how far to repair
// the item, a level at a time, then puts the vanilla crafting confirmation
// up with the components on it, or the power armor station's list of them where
// the bench is short. Saying yes spends them and pays the experience crafting a
// mod from them would. Slight wear is repaired on the spot for free, see
// FreeFromTenths, since a lock at full would otherwise make one shot a trip to
// the bench. An item the bench has nothing to rebuild from, see Cost.h, is sent
// to a trader. A worn item the bench would leave out of its list, since no mod
// fits it, is listed all the same and stays greyed, see Display.h.
//
// The weapon and armor benches are one menu class with different lists, so one
// set of hooks serves both. The button is the only way in. The game shares one
// button between REPAIR and RENAME, so the flag picking between them is
// written on every change of highlight.
//
// Bench.h holds the basics, Cost.h what a repair costs, Job.h the repair from
// the question to the finished item, Box.h its confirmation and what yes takes,
// Missing.h what a repair the bench cannot pay for shows, Supplies.h the oil,
// adhesive and cloth every repair takes, Lock.h the crafting perk ranks a
// repair that takes components needs, Label.h the button's word and the mods
// heading, Display.h how a worn item is shown and kept out of the slots,
// Lists.h the hooks on the lists that do it, and Spares.h and SpareBox.h the
// repair with a spare, another of the same weapon or armor, with any mods,
// taken apart. Inspect.h puts REPAIR on the Pip-Boy's inspect screen, see
// iPipboyRepair.
namespace Workbench
{
	// Patches the workbench menu's function table, each place by itself.
	void Install();

	// Says in NEC.log which repair supply the load order lacks, see
	// Supplies.h.
	void Load();

	// Forgets what the last bench listed and the word its button read, and
	// adds the listener that fades the equipped items and keeps that word to
	// the bench movie. Forgets the last Pip-Boy inspect too, see Inspect.h,
	// since it is the same movie. Ignores every other movie.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);

	// Whether the bench repairs: the 3 places behind the REPAIR button, the
	// flag, the grey and the press, all still run NEC's hooks. The mod lock
	// and the perk text ask it too, so they never outlive the repair.
	[[nodiscard]] bool Repairs();

	// Whether a weapons or armor workbench repairs: Repairs and
	// bWorkbenchRepair. When off the bench offers vanilla's RENAME and only
	// the mod lock holds. REPAIR on the Pip-Boy's inspect screen asks Repairs
	// alone, see RepairsIn in Bench.h. The bench tip and the perk text ask it.
	[[nodiscard]] bool BenchRepairs();

	// The condition an item has to be at to be modified: iModdingFrom kept
	// between 0 and 100, compared with the percent its card shows. At the
	// default of 100 a gun that has fired one shot reads 99% and is kept out
	// of the mod slots until it is repaired, which stops a cheap mod swap
	// before a repair. Below 100 the player has chosen to allow that, and at 0
	// even broken gear can be modded.
	[[nodiscard]] std::uint32_t ModifyFloor();

	// From this condition a repair is free and immediate, and the button reads
	// MEND. At the default floor of full, one shot closes the slots, and
	// charging for that would make every shot cost components. It takes
	// nothing off a real repair, since an item below it pays the whole way up
	// to full.
	// fFreeMendFrom in tenths of the number an item's card shows, 0 to 1000,
	// see Repair::CardTenthsFrom, so 97.5 by default mends from a card reading
	// 97.5%, or 98% at CND decimals Off. Worn gear reads 99.9% at most, or 99%
	// at Off, so 100 makes nothing free, and 99.5 too at Off. 0 frees
	// everything, broken gear included. A paid repair from fSuppliesOnlyFrom
	// up takes only its supplies, see Supplies.h, so by default an item from
	// 87.5% to 97.4% costs those.
	[[nodiscard]] std::uint32_t FreeFromTenths();

	// Whether any repair can take its supplies alone: while iRepairSupplies
	// is above 0, repairs cost something, fSuppliesOnlyFrom comes before
	// fFreeMendFrom on the card and the load order holds at least 1 supply.
	// SuppliesOnlyFromTenths and the loading screen's bench tip ask it.
	[[nodiscard]] bool SuppliesAlone();

	// The condition from which every paid repair takes only its supplies, at
	// any perk rank, in tenths of the card's number: fSuppliesOnlyFrom while
	// SuppliesAlone, else FreeFromTenths, since then no repair does. 875 by
	// default, or 880 at CND decimals Off.
	[[nodiscard]] std::uint32_t SuppliesOnlyFromTenths();

	// The components the bench would take from the player to bring a copy of
	// a_object, whose list is a_extra, from a_tenths, the number its card
	// shows in tenths, to a_to, a whole percent, as its confirmation lists
	// them, supplies first. Empty for a repair it mends
	// for nothing, nothing for one it would not make, such as one the repair
	// lock holds back. Priced on the scrap lists as last read, see
	// Materials::ReadScrapLists. Run it holding no inventory lock, with
	// a_extra kept alive by the caller.
	[[nodiscard]] std::optional<std::vector<Materials::Part>> BillFor(RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra,
		std::uint32_t a_tenths, std::uint32_t a_to);

	// The condition points the player's spare on the stack with a_spare
	// restores to the player's copy of a_object on the stack with a_item, as
	// the box listing the spares works it out, whole and rounded down, so 10
	// for a spare the box lists at +10.4%, or nothing when it is no spare
	// for it or while bLockSpares lets the repair lock hold it back. Run it
	// holding no inventory lock, with both lists kept alive by the caller.
	[[nodiscard]] std::optional<std::uint32_t> SpareGain(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_item,
		const RE::ExtraDataList* a_spare);

	// As the confirmation box a_menu opens, lets NEC's repair title show its
	// 2nd line, where a long item name pushes the level. Every other box keeps
	// the title as the game drew it.
	void FitRepairTitle(RE::ExamineConfirmMenu& a_menu);
}
