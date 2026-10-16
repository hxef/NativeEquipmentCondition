#pragma once

#include "Core/Plugin.h"

#include "Gameplay/SpawnCondition/SpawnCondition.h"

#include <utility>

// What marks a save being applied, a game recording no change, a console
// command, a script giving an item and a restock, and the scope that raises
// such a mark. Private to this folder.
namespace SpawnCondition
{
	// Sets a thread local while alive and puts back what it held, however the
	// call ends, so an inner call cannot clear an outer one's. A mark left up
	// would change what every weapon on that thread gets for the rest of the
	// session.
	template <class T>
	class Scoped
	{
	public:
		Scoped(T& a_slot, T a_value) :
			_slot(a_slot), _was(std::exchange(a_slot, a_value))
		{}
		~Scoped() { _slot = _was; }

		Scoped(const Scoped&) = delete;
		Scoped(Scoped&&) = delete;
		Scoped& operator=(const Scoped&) = delete;
		Scoped& operator=(Scoped&&) = delete;

	private:
		T&      _slot;
		const T _was;
	};

	// Whether a save is being applied: the game's loading flag, up for a whole
	// load, or this thread's bit, up while a form the save holds is read in
	// play. What arrives then is what the save holds, so nothing rolls. Read
	// live, 2 plain reads.
	bool Loading();

	// Whether the game records no change now, by its own flag or this thread's
	// bit, as while a new game sets up its world. A change written then is not
	// saved. With no save and load object the game records nothing, so that
	// counts too.
	bool NotRecording();

	// Whether this thread is part way through a console command, see
	// g_runningConsoleCommand in Guards.cpp.
	bool FromConsole();

	// Whether this inventory is one the player is filling from the console,
	// their own or whatever they clicked on. Narrower than a command running:
	// placeatme builds a whole character and its loadout in the same call,
	// and that loadout is ordinary loot. A character the console has clicked
	// counts as the player's pick, so gear resurrect or recycleactor gives it
	// again arrives new. Only reached while a command runs, on the game's own
	// thread, so reading the console's selection is safe.
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
	// to be before Roll writes it and SplitOff in Roll.cpp splits it after the
	// add. A stack built for one add lives on the caller's stack frame with no
	// smart pointer holding it. But the engine also copies every stack of an
	// item from one inventory into another, a workbench gathering its linked
	// containers for example, and the copy shares its extra data list with the
	// original, so splitting it would leave 2 inventories disagreeing about the
	// same weapons. A stack a save holds is already held by its item, so it
	// never counts either. A line the fill writes straight into a list is not
	// private but is rolled and split in place, since the fill gave it a fresh
	// extra data list, see RollPlainLines in SpawnCondition.cpp.
	bool Private(const RE::BGSInventoryItem::Stack& a_stack);

	// The restock this thread is part way through when a_list is the chest
	// being restocked, see ScopedRestock, or nothing.
	const Restock* Restocking(const RE::BGSInventoryList* a_list);

	// Patches the call that runs console commands and a script's AddItem and
	// RemoveItem, and says so in the log.
	void InstallGuards();
}
