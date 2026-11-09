#pragma once

#include "Core/Plugin.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

// The notes behind the 4 events other mods' scripts hear, see NEC.psc.in.
// The places that change an item push a note here as they happen, and 1
// task on F4SE's queue hands them on to the scripts, see Papyrus/Events.cpp.
// A script never runs inside the change that raised its event, and no note
// is handed on during a save load or while the game deletes every form.
//
// A push may come under an inventory lock: it takes only this queue's own
// lock, which takes nothing else. Kick queues the task, so it is never
// called under an inventory lock: F4SE holds its queue while a task runs,
// see Equipped.h, and the game would freeze.
namespace ScriptEvents
{
	enum class Kind : std::uint8_t
	{
		kConditionChanged,
		kBroke,
		kJammed,
		kFoodSpoiled,
	};

	// What changed the condition, the number a script reads as aiCause.
	enum class Cause : std::uint8_t
	{
		kWear = 0,
		kRepair = 1,
		kScript = 2,
		kRoll = 3,
		kConsole = 4,
	};

	using Clock = std::chrono::steady_clock;

	// 1 event waiting. It names the item by form IDs and holds the written
	// stack's list, so the list lives on and its address names no other
	// list while the note waits. The task finds the stack by that list.
	struct Note
	{
		Kind                                   kind{ Kind::kConditionChanged };
		Cause                                  cause{ Cause::kWear };
		RE::TESFormID                          holder{ 0 };  // 0 for an item lying in the world
		RE::TESFormID                          base{ 0 };    // the weapon, armor or food
		RE::ObjectRefHandle                    world;        // the item lying in the world, empty for a stack
		RE::BSTSmartPointer<RE::ExtraDataList> list;         // the stack's list, empty for a world item and a jam
		float                                  before{ -1.0F };  // healths, -1 for none
		float                                  after{ -1.0F };
		std::uint32_t                          count{ 0 };  // the pieces of food spoiled
		std::uint32_t                          game{ 0 };   // Game() as it was pushed
		std::uint32_t                          tries{ 0 };  // tasks that found the inventory busy
		Clock::time_point                      dropAt;      // when a busy note stops waiting
	};

	// -------------------------------------------------------------------
	// The gate
	// -------------------------------------------------------------------

	// Why an item function and a push do nothing right now: a save is
	// loading, the game is loading every file again, or the game has stopped
	// its scripts for a save, a load or a quit to the main menu. Nothing
	// while the game plays.
	[[nodiscard]] std::optional<std::string_view> Closed();

	// The game stopping its scripts, the byte the game sets on its script
	// VM before it waits for them to stop.
	[[nodiscard]] bool Freezing();

	// Closed for a save loading or the game loading every file, and not
	// only for Freezing.
	[[nodiscard]] bool Loading();

	// Goes up by 1 as a save loads, a new game begins, the main menu opens
	// and every form is deleted, so a note or a work made in 1 game is
	// dropped in the next.
	[[nodiscard]] std::uint32_t Game();

	// F4SE's kPreLoadGame closes the gate, before Forget, and kPostLoadGame
	// opens it.
	void CloseForLoad();
	void OpenAfterLoad();

	// The main menu opening, as it opens, before any task runs.
	void Bump();

	// Every note dropped and the count bumped, for a save load, a new game and
	// the main menu.
	void Forget();

	// kGameDataReady: false closes the gate and forgets, true opens it.
	void Unload();
	void Load();

	// -------------------------------------------------------------------
	// The sites
	// -------------------------------------------------------------------

	// The player's item at a_list changed from a_before to a_after. Anybody
	// else's is not heard.
	void Changed(const RE::TESObjectREFR& a_holder, const RE::TESBoundObject& a_base, RE::ExtraDataList* a_list, float a_before,
		float a_after, Cause a_cause);

	// A weapon or a piece of armor reached 0: a stack of a_holder by its
	// list, or with a_holder null the item a_world.
	void Broke(const RE::TESObjectREFR* a_holder, const RE::TESBoundObject& a_base, RE::ExtraDataList* a_list,
		RE::ObjectRefHandle a_world = {});

	// The player's gun jammed.
	void Jammed(const RE::TESObjectREFR& a_actor, const RE::TESBoundObject& a_weapon);

	// a_count pieces of a_food turned spoiled in a_holder's inventory, which
	// is null when the container has no reference.
	void Spoiled(const RE::TESObjectREFR* a_holder, const RE::TESBoundObject& a_food, std::uint32_t a_count);

	// A write on a_holder's stack at a_list: Changed, Broke when a_broke, and
	// the task queued. Only once the inventory's lock is let go.
	void Wrote(const RE::TESObjectREFR& a_holder, const RE::TESBoundObject& a_base, RE::ExtraDataList* a_list, float a_before,
		float a_after, Cause a_cause, bool a_broke);

	// Queues the task unless it is queued already.
	void Kick();

	// Kick, when notes wait. For places that run often.
	void KickIfWaiting();

	// -------------------------------------------------------------------
	// The task
	// -------------------------------------------------------------------

	// The task's work, Papyrus::RunTask. Set once as the plugin loads, since
	// this layer may not include Papyrus.
	void SetTask(void (*a_task)());

	// Every note, oldest first, for the task to hand on.
	[[nodiscard]] std::vector<Note> Take();

	// Notes the task could not hand on yet, back in front of any pushed
	// meanwhile, in their order.
	void PutBack(std::vector<Note>&& a_notes);

	// Notes dropped since the game started as more than 256 waited, for
	// NEC.papyrus.log.
	[[nodiscard]] std::uint32_t Overflowed();
}
