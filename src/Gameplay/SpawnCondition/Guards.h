#pragma once

#include "Core/Plugin.h"

// What marks a console command, a save loading and a script giving an item.
// Private to this folder.
namespace SpawnCondition
{
	// Whether this thread is part way through a console command, see
	// g_runningConsoleCommand in Guards.cpp.
	bool FromConsole();

	// Whether this inventory is one the player is filling from the console,
	// their own or whatever they clicked on. Narrower than a command running:
	// placeatme and resurrect build a whole character and its loadout in the
	// same call, and that loadout is ordinary loot. Only reached while a
	// command runs, on the game's own thread, so reading the console's
	// selection is safe.
	bool ConsoleTarget(const RE::BGSInventoryList* a_list);

	// Whether this thread is running a script's AddItem or RemoveItem, see
	// g_runningScriptTransfer in Guards.cpp. A quest pays its reward out this
	// way.
	bool FromScript();

	// Whether that script is handing over a gift: a quest item, or anything
	// a character gives away, see Giver in Guards.cpp. Asks a_extra whether
	// it is a quest item, which follows an item inside a container back to
	// its own reference.
	bool GiftFromScript(const RE::ExtraDataList& a_extra);

	// Whether nothing but the add in progress can reach a stack, which it has
	// to be before SplitOff in SpawnCondition.cpp splits it. A stack built for
	// one add lives on the caller's stack frame with no smart pointer holding
	// it, and a stack read from a save belongs to the loader. But the engine
	// also copies every stack of an item from one inventory into another, a
	// workbench gathering its linked containers for example, and the copy
	// shares its extra data list with the original, so splitting it would leave
	// 2 inventories disagreeing about the same weapons.
	bool Private(const RE::BGSInventoryItem::Stack& a_stack);

	// Patches the call that runs console commands, the call that loads a saved
	// inventory and a script's AddItem and RemoveItem, and says so in the log.
	void InstallGuards();
}
