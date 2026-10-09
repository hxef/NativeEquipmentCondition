#include "Gameplay/SpawnCondition/Guards.h"

#include "Condition/Condition.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/ItemCards.h"

#include <array>
#include <cstdint>
#include <utility>

namespace SpawnCondition
{
	namespace
	{
		// Whether this thread is part way through a console command. Asking
		// whether the console is open would be wrong: loading cells keeps
		// filling raiders and footlockers on other threads while it is open,
		// and asking the menu would take the global menu lock under the
		// inventory write lock, the deadlock ItemCards.h describes. A flag on
		// the working thread costs one read.
		thread_local bool g_runningConsoleCommand = false;

		// Sets a flag while alive and clears it however the call ends. A flag
		// left set would change what every weapon gets for the rest of the
		// session on that thread.
		struct Raise
		{
			// Restores the old value rather than clearing it, so an inner guard
			// cannot clear an outer call's flag. Sets nothing when told this
			// call does not count.
			explicit Raise(bool& a_flag, bool a_when = true) :
				flag(a_flag), previous(a_flag) { flag = flag || a_when; }
			~Raise() { flag = previous; }

			Raise(const Raise&) = delete;
			Raise& operator=(const Raise&) = delete;

		private:
			bool&      flag;
			const bool previous;
		};

		// The one call to Console::RunQueuedCommands in the game's update loop.
		// Typing a command only puts a Script in a queue, and this call is what
		// runs it, so it covers a typed command, a batch file and ForEachRef
		// alike.
		constexpr CallPatch::CallSite RUN_CONSOLE_COMMANDS_SITE[] = {
			{ 2228917, 0xDC4, "console commands" },
		};

		std::array<CallPatch::Link<void()>, 1> g_consoleLink;

		void RunConsoleCommandsHk()
		{
			const Raise running{ g_runningConsoleCommand, g_consoleLink[0].Live() };
			g_consoleLink[0]();
		}

		// Whether this thread is loading a saved inventory. A save keeps an
		// inventory as stacks, each added back through AddStack from an item
		// the loader owns, so nothing else can reach one.
		thread_local bool g_loadingSavedStack = false;

		// The one call that adds a saved stack, in the function that reads a
		// BGSInventoryList out of a save. It calls AddItem1, which takes the
		// write lock and calls AddStack.
		constexpr CallPatch::CallSite LOAD_SAVED_STACK_SITE[] = {
			{ 2194186, 0x0CC, "saved stack" },
		};

		std::array<CallPatch::Link<void(RE::BGSInventoryList*, RE::TESBoundObject*, RE::BGSInventoryItem::Stack*, std::uint32_t*, std::uint32_t*)>, 1> g_savedStackLink;

		void LoadSavedStackHk(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack,
			std::uint32_t* a_oldCount, std::uint32_t* a_newCount)
		{
			const Raise loading{ g_loadingSavedStack, g_savedStackLink[0].Live() };
			g_savedStackLink[0](a_list, a_object, a_stack, a_oldCount, a_newCount);
		}

		// Whether this thread is running a script's ObjectReference.AddItem or
		// RemoveItem. The script only queues a functor, and the game runs it
		// later on the main thread, so the flag goes up around the run and not
		// around the script.
		thread_local bool g_runningScriptTransfer = false;

		// Who that run takes its item from, worked out before it starts while
		// nothing is locked: the player, or a living character who does not
		// follow the player, see Giver.
		thread_local bool g_fromPlayer = false;
		thread_local bool g_fromCharacter = false;

		// Whether a character gives things away, the way Paladin Brandis hands
		// over his gun. A follower handing back what the player gave them and
		// a script taking a dead character's own gear give nothing.
		bool Giver(const RE::TESObjectREFR* a_source)
		{
			const auto actor = a_source ? a_source->As<RE::Actor>() : nullptr;
			return actor && actor != RE::PlayerCharacter::GetSingleton() && !actor->IsDead(true) && !actor->IsPlayerTeammate();
		}

		// Where AddItem takes its item from. A base form is new and comes from
		// nobody. With inventoryItem set, the item names the container holding
		// it. A reference on its own is in a container or out in the world.
		RE::NiPointer<RE::TESObjectREFR> Source(const RE::GameScript::AddItemFunctor& a_functor)
		{
			const auto form = RE::TESForm::GetFormByID(a_functor.item);
			const auto item = form ? form->IsReference() : nullptr;
			if (!item || a_functor.inventoryItem != 0) {
				return RE::NiPointer<RE::TESObjectREFR>{ item };
			}
			const auto handles = item->extraList ? item->extraList->GetByType<RE::ExtraReferenceHandles>() : nullptr;
			return handles ? handles->containerRef.get() : nullptr;
		}

		// The call operators. Each returns a Variable through a pointer after
		// this, the way a member function returning a class does.
		REL::Relocation<RE::BSScript::Variable* (*)(RE::GameScript::AddItemFunctor*, RE::BSScript::Variable*)>    _RunScriptAddItem;
		REL::Relocation<RE::BSScript::Variable* (*)(RE::GameScript::RemoveItemFunctor*, RE::BSScript::Variable*)> _RunScriptRemoveItem;

		// The 2 call operators. Each carries a Link for the proof, though it
		// hands on through the kept pointer above.
		CallPatch::Held                                                                                        g_scripts;
		CallPatch::Link<RE::BSScript::Variable* (*)(RE::GameScript::AddItemFunctor*, RE::BSScript::Variable*)>    g_addItemLink;
		CallPatch::Link<RE::BSScript::Variable* (*)(RE::GameScript::RemoveItemFunctor*, RE::BSScript::Variable*)> g_removeItemLink;

		RE::BSScript::Variable* RunScriptAddItemHk(RE::GameScript::AddItemFunctor* a_functor, RE::BSScript::Variable* a_result)
		{
			if (!g_scripts.Runs(g_addItemLink)) {
				return _RunScriptAddItem(a_functor, a_result);
			}
			const auto  source = Source(*a_functor);
			const Raise running{ g_runningScriptTransfer };
			const Raise fromPlayer{ g_fromPlayer, source.get() == RE::PlayerCharacter::GetSingleton() };
			const Raise fromCharacter{ g_fromCharacter, Giver(source.get()) };
			return _RunScriptAddItem(a_functor, a_result);
		}

		RE::BSScript::Variable* RunScriptRemoveItemHk(RE::GameScript::RemoveItemFunctor* a_functor, RE::BSScript::Variable* a_result)
		{
			if (!g_scripts.Runs(g_removeItemLink)) {
				return _RunScriptRemoveItem(a_functor, a_result);
			}
			const auto  source = a_functor->source.get();
			const Raise running{ g_runningScriptTransfer };
			const Raise fromPlayer{ g_fromPlayer, source.get() == RE::PlayerCharacter::GetSingleton() };
			const Raise fromCharacter{ g_fromCharacter, Giver(source.get()) };
			return _RunScriptRemoveItem(a_functor, a_result);
		}

		// The restock this thread is part way through, see ScopedRestock. A
		// trader's chest restocks inside the barter screen's own call, on the
		// thread that runs it.
		thread_local const Restock* t_restock = nullptr;
	}

	bool FromConsole()
	{
		return g_runningConsoleCommand;
	}

	bool FromScript()
	{
		return g_runningScriptTransfer;
	}

	// A quest item counts from anyone but the player, a quest's chest
	// included, since only a quest puts one anywhere: the player cannot drop
	// or store a quest item. The flag that makes one is on the alias, so an
	// alias a display rack's script keeps the player's own gun in proves
	// nothing. Asked under the inventory lock, the way
	// BGSInventoryList::GetQuestItemCount asks it under its own, so the lock
	// order stays inventory then extra data.
	bool GiftFromScript(const RE::ExtraDataList& a_extra)
	{
		return g_runningScriptTransfer && !g_fromPlayer && (g_fromCharacter || a_extra.IsQuestObject());
	}

	bool ConsoleTarget(const RE::BGSInventoryList* a_list)
	{
		if (!a_list) {
			return false;
		}

		const auto owner = a_list->owner.get();
		if (!owner) {
			return false;
		}

		if (owner.get() == static_cast<const RE::TESObjectREFR*>(RE::PlayerCharacter::GetSingleton())) {
			return true;
		}

		// Compared as handles so that nothing has to be resolved twice.
		const auto picked = RE::Console::GetCurrentPickREFR();
		return picked && picked == a_list->owner;
	}

	bool Private(const RE::BGSInventoryItem::Stack& a_stack)
	{
		return a_stack.QRefCount() == 0 || g_loadingSavedStack;
	}

	ScopedRestock::ScopedRestock(const Restock& a_restock) :
		_was(std::exchange(t_restock, &a_restock))
	{}

	ScopedRestock::~ScopedRestock()
	{
		t_restock = _was;
	}

	// Compared as handles, like Players in Owners.cpp, since this is
	// asked under the inventory write lock for every weapon and piece of
	// armor that comes in.
	const Restock* Restocking(const RE::BGSInventoryList* a_list)
	{
		return t_restock && a_list && a_list->owner == t_restock->chest ? t_restock : nullptr;
	}

	void KeepCarried()
	{
		// A save loading hands every stack it reads to AddStackHk, and a full
		// reset is deleting every form, so neither is the time.
		const auto* main = RE::Main::GetSingleton();
		const auto* ui = RE::UI::GetSingleton();
		if ((main && main->resetGame) || (ui && ui->GetMenuOpen<RE::LoadingMenu>())) {
			return;
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* inv = player ? player->inventoryList : nullptr;
		if (!inv) {
			return;
		}

		// Stamped the way a script's gift is, see Roll. Only a stack whose
		// extra data list nothing else holds: one another inventory shares is
		// not the player's alone, see Private.
		std::uint32_t kept = 0;
		{
			const RE::BSAutoWriteLock l(inv->rwLock);
			inv->ForEachStack(
				[](RE::BGSInventoryItem& a_item) { return a_item.object && Condition::WearsOut(*a_item.object); },
				[&kept](RE::BGSInventoryItem&, RE::BGSInventoryItem::Stack& a_stack) {
					if (a_stack.extra && a_stack.extra->QRefCount() == 1 && a_stack.extra->GetHealthPerc() < 0.0F) {
						a_stack.extra->SetHealthPerc(Condition::MAX_HEALTH);
						kept++;
					}
					return true;
				});
		}

		// Outside the lock, see ItemCards.h.
		if (kept > 0) {
			ItemCards::Refresh(RE::ENUM_FORM_ID::kWEAP);
			ItemCards::Refresh(RE::ENUM_FORM_ID::kARMO);
		}
		REX::INFO("Worn loot is back on. {:d} stacks you carry that arrived while it was off keep their condition.", kept);
	}

	void InstallGuards()
	{
		// If this patch fails, console items just roll like any other loot, so
		// it is reported and everything else carries on.
		const auto consoleHooks = CallPatch::PerSite<std::size(RUN_CONSOLE_COMMANDS_SITE)>([]<std::size_t I>() { return &RunConsoleCommandsHk; });
		if (CallPatch::PatchAll(RUN_CONSOLE_COMMANDS_SITE, RE::ID::Console::RunQueuedCommands, consoleHooks, g_consoleLink,
				"Console commands hand out weapons and armor at full condition", Part::kConsoleNew) == 0) {
			REX::WARN("Weapons and armor added from the console will be rolled like any other loot.");
		}

		// If this patch fails, an older save's stacks just keep sharing one
		// condition, so it is reported the same way.
		const auto savedHooks = CallPatch::PerSite<std::size(LOAD_SAVED_STACK_SITE)>([]<std::size_t I>() { return &LoadSavedStackHk; });
		if (CallPatch::PatchAll(LOAD_SAVED_STACK_SITE, RE::ID::BGSInventoryList::AddItem1, savedHooks, g_savedStackLink,
				"Weapons and armor a save kept in one stack split up as it loads", Part::kOldSaves) == 0) {
			REX::WARN("Weapons and armor a save from before this mod kept in one stack will share one condition.");
		}

		// Index 1 is the call operator. Every script AddItem and RemoveItem
		// runs through one, whatever the script is attached to.
		const CallPatch::Together scripts{ Part::kGifts };
		REL::Relocation<std::uintptr_t> addItem{ RE::GameScript::AddItemFunctor::VTABLE[0] };
		_RunScriptAddItem = CallPatch::PatchSlot(addItem, 0x01, RunScriptAddItemHk, "script additem", Part::kNone, true, &g_addItemLink).value_or(0);
		REL::Relocation<std::uintptr_t> removeItem{ RE::GameScript::RemoveItemFunctor::VTABLE[0] };
		_RunScriptRemoveItem = CallPatch::PatchSlot(removeItem, 0x01, RunScriptRemoveItemHk, "script removeitem", Part::kNone, true, &g_removeItemLink).value_or(0);
		g_scripts = scripts.Set();
		if (!g_scripts) {
			REX::WARN("Weapons and armor a script gives the player will be rolled like any other loot.");
			return;
		}
		REX::INFO("Weapons and armor a script gives the player with no condition yet, a quest reward among them, arrive at full condition.");
		REX::INFO("So does a quest item or a character's gift a script hands the player.");
	}
}
