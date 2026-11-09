#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <vector>

// What a script's write leaves for later, and the task that runs it and hands
// the waiting events to scripts, see ScriptEvents.h. Private to this folder.
namespace Papyrus
{
	// What may not run under an inventory lock or inside the script's call.
	// Form IDs and a handle only, never a stack, which can be gone by then.
	struct Work
	{
		std::uint32_t       game{ 0 };  // ScriptEvents::Game() as the call began
		RE::ObjectRefHandle holder;
		RE::TESFormID       base{ 0 };
		bool                cards{ false };    // the player's item, whose cards the Pip-Boy keeps
		bool                armor{ false };    // a worn piece, whose protection is added up again
		bool                putAway{ false };  // the player's weapon in hand, written to 0
	};

	// Keeps a_work for the task and queues the task. Not under a lock.
	void Queue(const Work& a_work);

	// The works kept so far, in their order, taken off the queue.
	std::vector<Work> TakeWorks();

	// Puts a_works back ahead of any kept since, for the task's next run.
	void PutWorksBack(std::vector<Work>&& a_works);

	// Refreshes what a_work names, unless it is from the last game.
	void RunWork(const Work& a_work);

	// The task's run, which ScriptEvents queues.
	void RunTask();
}
