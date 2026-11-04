#pragma once

#include "Core/Plugin.h"

#include "UI/Repair/Workbench/Cost.h"

// The box of spares and what a pick does. Private to this folder.
//
// USE A SPARE in the repair box opens it: headed by the vanilla REPAIR, a row
// per spare with what it restores, see Spares.h, and CANCEL last, which the
// Cancel key presses too and which leaves the player at the bench. A pick
// takes that spare apart at once: the item is repaired, the parts the repair
// does not need go to the player, the crafting sound plays and the bench
// shows the item's new condition with the spare gone from its list. Then the
// box comes back with every row worked out again, until the item is at full
// or no spare is left. A pick that lifts the item above iFreeMendAbove mends
// the rest for free, as MEND would.
namespace Workbench
{
	// Puts the box up for the player's copy of a_object on the stack with
	// a_extra, priced by a_priced, what PriceOf read at the REPAIR press.
	// Nothing for an item at full or with no spare left. Each pick comes back
	// a moment later from the task queue.
	void AskWhichSpare(RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra, const Priced& a_priced);
}
