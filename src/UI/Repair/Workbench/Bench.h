#pragma once

#include "Core/Plugin.h"

#include "UI/Repair/SelectedItem.h"
#include "UI/Repair/Workbench/Workbench.h"

#include <cstdint>
#include <optional>

// The basics the weapon and armor bench code uses. Private to this folder.
namespace Workbench
{
	// True while fBenchCostMult is 0 or below. A repair then costs nothing, so
	// every repair works like MEND.
	[[nodiscard]] bool FreeRepairs();

	// The vanilla word REPAIR, which the game translates: what it builds the
	// bench's REPAIR button with, and the button of a repair's confirmation.
	inline constexpr const char* REPAIR_WORD = "$REPAIR";

	// The item highlighted in the bench's list, with the bench's own 2 tests.
	struct Selection : SelectedItem::Item
	{
		// True for an item too worn for the bench to modify.
		[[nodiscard]] bool TooWorn() const { return object && percent < ModifyFloor(); }

		// True for an item worn so little that repairing it is free, its card
		// from FreeFromTenths up, and for any worn item while repairs cost
		// nothing.
		[[nodiscard]] bool Trifling() const { return Worn() && (FreeRepairs() || tenths >= FreeFromTenths()); }
	};

	// What the highlight is on, or nothing while the bench has no list. The
	// armor bench is this same menu with armor in its list, so it reads the
	// same way. The chem and cooking stations share the menu too, and nothing
	// there wears out, which keeps REPAIR off them.
	[[nodiscard]] Selection Selected(RE::ExamineMenu* a_menu);

	// iPipboyRepair as it stands: 0 off, 1 with a spare, 2 as at a workbench.
	// Any other number reads as 0, and 1 reads as 0 while bSpareRepair is off.
	[[nodiscard]] std::int32_t PipboyRepair();

	// Whether a_menu is the inspect screen of an item the player looks at
	// from the Pip-Boy: inspect mode with no featured item and no bench, the
	// Pip-Boy open under it, and no container or barter screen open, whose
	// inspect screen is the same menu.
	[[nodiscard]] bool PipboyInspect(RE::ExamineMenu* a_menu);

	// The item on show in a Pip-Boy inspect, read as Selected reads it, or
	// nothing anywhere else. PREV and NEXT change it with the screen open.
	[[nodiscard]] Selection Inspected(RE::ExamineMenu* a_menu);

	// The item a repair works on: Selected at a bench, Inspected in a Pip-Boy
	// inspect while PipboyRepair is 2, and nothing otherwise.
	[[nodiscard]] Selection Repairing(RE::ExamineMenu* a_menu);

	// Whether a_menu repairs now. At a bench BenchRepairs. In a Pip-Boy
	// inspect Repairs, NEC on, PipboyRepair above 0 and the inspect key place
	// live, since REPAIR there rides on the bench's 3 places and its box code,
	// whatever bWorkbenchRepair says. False anywhere else.
	[[nodiscard]] bool RepairsIn(RE::ExamineMenu* a_menu);

	// The press of REPAIR or MEND on a_selection, the bench's way into a
	// repair. A barely worn item is repaired on the spot. One the bench has
	// nothing to rebuild from is refused, with a trader named while trader
	// repairs are on, see Cost.h, and one the repair lock holds back is
	// turned away or shown its spares, see Lock.h. One with a single level
	// worth offering goes straight to the confirmation, through the task
	// queue like the prompt, unless the player carries a spare for it, see
	// Spares.h, which only the question offers. The bench's button and REPAIR
	// on the Pip-Boy's inspect screen while iPipboyRepair is 2 both run it.
	void BenchPress(RE::ExamineMenu* a_menu, const Selection& a_selection);

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
