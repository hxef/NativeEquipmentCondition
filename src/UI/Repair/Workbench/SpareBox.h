#pragma once

#include "Core/Plugin.h"

#include "UI/Repair/Workbench/Cost.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

// The box of spares and what a pick does. Private to this folder.
//
// USE A SPARE in the repair box opens it, and so does REPAIR while the repair
// lock holds the item back, see Lock.h, with the lock's line over the
// question, since a spare repairs whatever the lock says while bLockSpares is
// off. It is headed by the vanilla REPAIR, a row per spare with what it
// restores, see Spares.h, and CANCEL last, which the Cancel key presses too
// and which leaves the player at the bench. A pick takes that spare apart
// at once: the item is repaired, the components the repair does not need
// go to the player, the crafting sound plays and the bench shows the item's
// new condition with the spare gone from its list. Then the box comes back
// with every row worked out again, until the item is at full or no spare is
// left. A pick that lifts the item to fFreeMendFrom or more mends the rest
// for free, as MEND would. At iKeepSpareMods 3 and 4 a spare with a mod that
// comes off first opens the box that picks its mods, see SpareModBox.h.
//
// REPAIR on the Pip-Boy's inspect screen opens it too while iPipboyRepair is
// 1, and USE A SPARE in its repair box while it is 2, see Inspect.h. At 1 no
// repair there takes components, so the box shows no lock's line.
namespace Workbench
{
	// Puts the box up for the player's copy of a_object on the stack with
	// a_extra, priced by a_priced, what PriceOf read at the REPAIR press.
	// Nothing for an item at full or with no spare left. Each pick comes back
	// a moment later from the task queue.
	void AskWhichSpare(RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra, const Priced& a_priced);

	// Uses up the spare on a_spare's stack on the item on a_item's stack,
	// from the task queue. The spare goes before the item is written, so a
	// spare that cannot be taken repairs nothing. A spare in the item's own
	// stack goes after, from what is left of that stack once the item has
	// split off it. a_plan names the mods that come back, see SpareMods.h,
	// and with none given iKeepSpareMods picks them. Then the box of spares
	// comes back while the item is not at full.
	void Pick(RE::TESBoundObject& a_object, const RE::ExtraDataList* a_item, const RE::ExtraDataList* a_spare,
		const Priced& a_priced, const std::optional<std::vector<std::uint32_t>>& a_plan);

	// The list of the stack the item was written on. A stack of 1 is
	// written in place. A bigger one splits 1 copy off right after
	// itself, with a list of its own. A written copy that matches another
	// stack merges into it, so the first stack whose card reads a_tenths,
	// the condition written in tenths, with the item's name is the fallback.
	// Nothing when none is found.
	[[nodiscard]] const RE::ExtraDataList* Written(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_was, bool a_split,
		std::uint32_t a_tenths, std::string_view a_name);

	// Brings a_menu up to date after a repair of a_object, see Refresh in
	// Job.h, with the highlight kept on the stack with a_written, see
	// Written. In a Pip-Boy inspect, whose record names the item the Pip-Boy
	// first asked for, a record that could not be written is put on no
	// stack, so the list keeps the row it is on by its number.
	void RefreshKept(RE::ExamineMenu& a_menu, const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_written);
}
