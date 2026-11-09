#pragma once

#include "Core/Plugin.h"

#include "Condition/Materials/Materials.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

// Repairing a worn weapon or piece of armor at its workbench. An item below
// the mod lock, full unless fModLockStart says less, cannot be modified: its
// row is faded, it gets no mod slots, the heading over them goes, and asking
// for them says why. Instead a REPAIR button on the bar asks how far to repair
// the item, a level at a time, then puts the vanilla crafting confirmation
// up with the components on it, or the power armor station's list of them where
// the bench is short. Saying yes spends them and pays the experience crafting a
// mod from them would. Slight wear is repaired on the spot for free, see
// FreeAbove, since a lock at full would otherwise make one shot a trip to the
// bench. An item the bench has nothing to rebuild from, see Cost.h, is sent to
// a trader. A worn item the bench would leave out of its list, since no mod
// fits it, is listed all the same and stays greyed, see Display.h.
//
// The weapon and armor benches are one menu class with different lists, so one
// set of hooks serves both. The button is the only way in. The game shares one
// button between REPAIR and RENAME, so the flag picking between them is
// written on every change of highlight.
//
// Bench.h holds the basics, Cost.h what a repair costs, Job.h the repair from
// the question to the finished item, Box.h its confirmation and what yes takes,
// Missing.h what a repair the bench cannot pay for shows, Label.h the button's
// word and the mods heading, Display.h how a worn item is shown and kept out of
// the slots, Lists.h the hooks on the lists that do it, and Spares.h and
// SpareBox.h the repair with a spare, another of the same weapon or armor,
// with any mods, taken apart.
namespace Workbench
{
	// Patches the workbench menu's function table, each place by itself.
	void Install();

	// Forgets what the last bench listed and the word its button read, and
	// adds the listener that fades the equipped items and keeps that word to
	// the bench movie. Ignores every other movie.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);

	// Whether the bench repairs: the 3 places behind the REPAIR button, the
	// flag, the grey and the press, all still run NEC's hooks. The mod lock
	// and the perk text ask it too, so they never outlive the repair.
	[[nodiscard]] bool Repairs();

	// The condition an item has to be at to be modified, a whole percent from
	// fModLockStart. At the default of full a gun that has fired one shot is
	// kept out of the mod slots until it is repaired, which stops a cheap mod
	// swap before a repair. Below full the player has chosen to allow that.
	[[nodiscard]] std::uint32_t ModifyFloor();

	// Above this condition a repair is free and immediate, and the button reads
	// MEND. At the default floor of full, one shot closes the slots, and
	// charging for that would make every shot cost components. It takes
	// nothing off a real repair, since an item below it pays the whole way up
	// to full.
	// iFreeMendAbove kept between 0 and 100, see Settings.h, so at 100 nothing
	// is free and at 0 everything but a broken item is.
	[[nodiscard]] std::uint32_t FreeAbove();

	// The components the bench would take from the player to bring a copy of
	// a_object, whose list is a_extra, from a_from to a_to, whole percents, as
	// its confirmation lists them. Empty for a repair it mends for nothing,
	// nothing for one it would not make. Priced on the scrap lists as last
	// read, see Materials::ReadScrapLists. Run it holding no inventory lock,
	// with a_extra kept alive by the caller.
	[[nodiscard]] std::optional<std::vector<Materials::Part>> BillFor(RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra,
		std::uint32_t a_from, std::uint32_t a_to);

	// The condition points the player's spare on the stack with a_spare
	// restores to the player's copy of a_object on the stack with a_item, as
	// the box listing the spares works it out, or nothing when it is no spare
	// for it. Run it holding no inventory lock, with both lists kept alive by
	// the caller.
	[[nodiscard]] std::optional<std::uint32_t> SpareGain(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_item,
		const RE::ExtraDataList* a_spare);

	// As the confirmation box a_menu opens, lets NEC's repair title show its
	// 2nd line, where a long item name pushes the level. Every other box keeps
	// the title as the game drew it.
	void FitRepairTitle(RE::ExamineConfirmMenu& a_menu);
}
