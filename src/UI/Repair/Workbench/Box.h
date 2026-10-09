#pragma once

#include "Core/Plugin.h"

// A paid repair's confirmation box, and what yes takes from the bench. Private
// to this folder.
//
// NEC puts the box up itself, built the way the game's TryCreate builds a
// mod's: "Repair" as the question, REPAIR on the button and a row per
// component, every word the game's own. The callback behind it is NEC's, so
// yes and no reach the job without passing any slot of the bench's table,
// and another DLL mod hooking the bench's build cannot turn repairs off.
namespace Workbench
{
	// Puts the box up for the job in hand. The job lasts as long as the box's
	// callback: yes runs Finish, and no or the bench closing drops the job.
	// A box that did not go up drops it at once.
	void ShowRepairBox(RE::ExamineMenu* a_menu);

	// Takes the job's components from every container the bench spends from,
	// the player's inventory among them, in the order the game's
	// ConsumeSelectedItems takes a mod's. Junk gives back what the repair does
	// not use, as scrap.
	void Spend(RE::ExamineMenu* a_menu);

	// Plays the bench's crafting loop, as building a mod does, over the
	// crafting sounds of what was queued for it, or the put down sound of an
	// item without one. The loop stops when the last of them ends, so with
	// nothing queued it does not play at all.
	void PlayRepairSound(RE::ExamineMenu* a_menu);
}
