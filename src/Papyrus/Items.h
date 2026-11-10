#pragma once

#include "Core/Plugin.h"

#include "Papyrus/Log.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

// How a script names an item, and finding it again at every call. Nothing of a
// stack is kept from one call to the next, since the game splits, merges and
// frees stacks on its own threads. Private to this folder.
namespace Papyrus
{
	// An item as a script hands it over: a reference lying in the world, a
	// persistent reference inside a container, or 1 stack inside a container
	// by the container and the stack's number, see Condition/UniqueID.h.
	using Item = RE::GameScript::RefrOrInventoryObj;

	// What an item names. The pointers are good only while the Holding that
	// found them lives.
	struct Found
	{
		// The container or actor holding the stack, null for an item lying in
		// the world.
		RE::NiPointer<RE::TESObjectREFR> holder;

		// The world reference, null for a stack.
		RE::NiPointer<RE::TESObjectREFR> world;

		RE::TESBoundObject* base{ nullptr };

		// The stack's list, or the world reference's own.
		RE::ExtraDataList* extra{ nullptr };

		// The number the stack was named by, 0 for a reference.
		std::uint16_t uid{ 0 };

		// How many stacks hold that number, 1 unless a split copied it. The
		// first in list order is the one named, as every lookup of the engine
		// takes it.
		std::uint32_t matches{ 0 };

		// The named stack's entry and its place in the entry's stacks, null
		// and 0 for a reference lying in the world.
		const RE::BGSInventoryItem* entry{ nullptr };
		std::uint32_t               stack{ 0 };

		std::uint32_t count{ 0 };
		bool          equipped{ false };

		// What the holder's wear of this item is multiplied by, a
		// companion's or a settler's share for its kind of gear, see
		// Condition/Crew.h. 1 for anyone else.
		float crew{ 1.0F };
	};

	// Finds what a_item names and, for a stack, holds the holder's inventory
	// as a writer until it goes, so no other thread changes the stack
	// meanwhile. The engine's own readers and writers count this thread in,
	// so code that takes this same inventory's lock may run inside. Code
	// that takes another lock may not, see Core/ItemCards.h. The lock is
	// tried once and never waited on, since a thread holding it may wait
	// for this one, and a script's call that finds it held runs again at the
	// VM's next update, see Waiting.h. found() is null for None, a gone
	// item, a locked inventory, one the game has not loaded, and while a
	// save loads or the game froze its scripts, see Core/ScriptEvents.h. The
	// trace log says which, and NEC.papyrus.log counts each but a locked
	// inventory and one not loaded, after the lock is gone.
	class Holding
	{
	public:
		explicit Holding(const Item& a_item);
		~Holding();

		Holding(const Holding&) = delete;
		Holding& operator=(const Holding&) = delete;

		[[nodiscard]] Found* found() noexcept { return _ok ? &_found : nullptr; }

		// For a write: true for gear NEC gives a condition, else a trace line
		// and a warning. Only once found() is not null.
		[[nodiscard]] bool Writable();

		// A warning for NEC.papyrus.log, written once the lock is gone.
		void WarnLater(Log::Warning a_warning, std::string a_detail);

	private:
		Found                       _found;
		RE::BGSInventoryList*       _locked{ nullptr };
		bool                        _ok{ false };
		std::optional<Log::Warning> _warning;
		std::string                 _detail;
	};

	// Names 1 stack of a_holder for an event: the one whose list is a_list,
	// or with a_list null the first equipped stack of a_base, numbered as
	// GetItems numbers it. a_inside runs once, on its handle under the lock,
	// or on None when the stack is gone, and answers whether it used the
	// handle: a number given here and not used is taken back. The lock is
	// tried once a round and never waited for, for F4SE's queue. False while
	// the inventory is busy, and a_inside has not run.
	[[nodiscard]] bool NameStack(RE::TESObjectREFR& a_holder, const RE::ExtraDataList* a_list, const RE::TESBoundObject& a_base,
		const std::function<bool(const Item&)>& a_inside);

	// Whether an item call is answered now, else a trace line saying why
	// not: a save is loading or the game froze its scripts, see
	// Core/ScriptEvents.h.
	[[nodiscard]] bool Answering();

	// The trace log's words: an item as "item 2 in container 00000014", "ref
	// FF014729" or "none", a form as 8 hex digits or "none", a health with 9
	// digits, which tells MAX_HEALTH and UNROLLED apart, and a percent.
	[[nodiscard]] std::string Shown(const Item& a_item);
	[[nodiscard]] std::string Hex(const RE::TESForm* a_form);

	// A form as NEC.papyrus.log names it, by its name and form ID: "Frag
	// Grenade [000EEBED]".
	[[nodiscard]] std::string FormName(const RE::TESForm& a_form);
	[[nodiscard]] std::string ShownHealth(float a_health);

	// Every stack of a_base in a_inv, for the trace line after a write, so a
	// reader sees that 1 copy changed and 1 stack holds each number. Under the
	// lock.
	[[nodiscard]] std::string RowsOf(const RE::BGSInventoryList& a_inv, const RE::TESBoundObject& a_base);

	// True for gear NEC gives a condition, else a trace line saying why not.
	[[nodiscard]] bool Tracked(const Found& a_found);

	// The trace line of a true or false answer, and the answer.
	bool Answer(bool a_ok);

	// A health as the script reads it, a percent from 0 to 100, 100 for a
	// stack with no health or a mark that it has not rolled yet.
	[[nodiscard]] float PercentOf(float a_health);

	// A percent a script hands over as the health NEC's curves read, kept
	// between 0 and 100, 100 as MAX_HEALTH.
	[[nodiscard]] float HealthAt(float a_percent);

	// True for a stack an NPC holds, which wears at the NPC rate.
	[[nodiscard]] bool HeldByNpc(const Found& a_found);

	// What 1 use costs the copy whose list is a_extra, as a health, times
	// a_crew, the holder's share in Found. Armor takes hits as hard as the
	// ordinary weapon's. Under the lock that keeps a_extra.
	[[nodiscard]] float PerUseOf(const RE::TESBoundObject& a_base, const RE::ExtraDataList* a_extra, bool a_theirs, float a_crew);
}
