#pragma once

#include "Core/Plugin.h"

#include "UI/Repair/SelectedItem.h"

#include <cstdint>

// The basics the weapon workbench code uses. Private to this folder.
namespace Workbench
{
	// The condition a weapon has to be at to be modified. Full, so a gun that
	// has fired one shot is kept out of the mod slots until it is repaired.
	inline constexpr std::uint32_t MODIFY_FLOOR = 100;

	// Above this condition a repair is free and immediate, and the button reads
	// MEND. The floor is at full, so one shot closes the slots, and charging
	// for that would make every shot cost components. It takes nothing off a
	// real repair, since a gun at 90 pays the whole last 10.
	inline constexpr std::uint32_t FREE_ABOVE = 95;

	// REPAIR in the game's own words, which it translates: what it builds the
	// bench's REPAIR button with, and the button of a repair's confirmation.
	inline constexpr const char* REPAIR_WORD = "$REPAIR";

	// The weapon highlighted in the bench's list, with the bench's own 2 tests.
	struct Selection : SelectedItem::Item
	{
		// True for a weapon too worn for the bench to modify.
		[[nodiscard]] bool TooWorn() const { return object && percent < MODIFY_FLOOR; }

		// True for a weapon worn so little that repairing it is free.
		[[nodiscard]] bool Trifling() const { return Worn() && percent > FREE_ABOVE; }
	};

	// What the highlight is on, or nothing while the bench has no list. The
	// chem, cooking and armor stations share the menu, and nothing there wears
	// out, which keeps REPAIR off them.
	[[nodiscard]] Selection Selected(RE::ExamineMenu* a_menu);

	// The weapon bench on the screen, or nothing if the player walked away.
	// Asked rather than remembered, since the answer is wanted a moment after
	// the player chose a level.
	[[nodiscard]] RE::ExamineMenu* OpenBench();

	// Builds the bench's working copy of the selected weapon again. The card is
	// drawn from a copy the bench takes on a change of highlight, extra data
	// and all, so a repair has to ask for it.
	void RebuildModdedItem(RE::ExamineMenu* a_menu);

	// Whether the bench is on a repair. The flag is the game's own, and only
	// the power armor station sets it otherwise.
	[[nodiscard]] bool Repairing(RE::ExamineMenu* a_menu);
}
