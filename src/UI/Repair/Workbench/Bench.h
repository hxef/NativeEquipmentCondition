#pragma once

#include "Core/Plugin.h"

#include "UI/Repair/SelectedItem.h"

#include <cstdint>
#include <optional>

// The basics the weapon and armor bench code uses. Private to this folder.
namespace Workbench
{
	// The condition an item has to be at to be modified. Full, so a gun that
	// has fired one shot is kept out of the mod slots until it is repaired, and
	// a chest piece that took one hit the same.
	inline constexpr std::uint32_t MODIFY_FLOOR = 100;

	// Above this condition a repair is free and immediate, and the button reads
	// MEND. The floor is at full, so one shot closes the slots, and charging
	// for that would make every shot cost components. It takes nothing off a
	// real repair, since an item below it pays the whole way up to full.
	// iFreeMendAbove kept between 0 and 100, see Settings.h, so at 100 nothing
	// is free and at 0 everything but a broken item is.
	[[nodiscard]] std::uint32_t FreeAbove();

	// True while fBenchCostMult is 0 or below. A repair then costs nothing, so
	// every repair works like MEND.
	[[nodiscard]] bool FreeRepairs();

	// REPAIR in the game's own words, which it translates: what it builds the
	// bench's REPAIR button with, and the button of a repair's confirmation.
	inline constexpr const char* REPAIR_WORD = "$REPAIR";

	// The item highlighted in the bench's list, with the bench's own 2 tests.
	struct Selection : SelectedItem::Item
	{
		// True for an item too worn for the bench to modify.
		[[nodiscard]] bool TooWorn() const { return object && percent < MODIFY_FLOOR; }

		// True for an item worn so little that repairing it is free, and for
		// any worn item while repairs cost nothing.
		[[nodiscard]] bool Trifling() const { return Worn() && (FreeRepairs() || percent > FreeAbove()); }
	};

	// What the highlight is on, or nothing while the bench has no list. The
	// armor bench is this same menu with armor in its list, so it reads the
	// same way. The chem and cooking stations share the menu too, and nothing
	// there wears out, which keeps REPAIR off them.
	[[nodiscard]] Selection Selected(RE::ExamineMenu* a_menu);

	// What the bench works on, by the workbench's own type: weapons at the
	// weapon bench and armor at the armor bench. Nothing at a chem or cooking
	// station, or for an item inspected from the Pip-Boy, which has no bench.
	[[nodiscard]] std::optional<Condition::Kind> WorksOn(RE::ExamineMenu* a_menu);

	// The bench on the screen, or nothing if the player walked away. Asked
	// rather than remembered, since the answer is wanted a moment after the
	// player chose a level.
	[[nodiscard]] RE::ExamineMenu* OpenBench();

	// Builds the bench's working copy of the selected item again. The card is
	// drawn from a copy the bench takes on a change of highlight, extra data
	// and all, so a repair has to ask for it.
	void RebuildModdedItem(RE::ExamineMenu* a_menu);
}
