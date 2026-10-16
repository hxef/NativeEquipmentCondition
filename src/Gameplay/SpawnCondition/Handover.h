#pragma once

#include "Core/Plugin.h"

#include "Gameplay/SpawnCondition/Band.h"
#include "Gameplay/SpawnCondition/Guards.h"

#include <optional>

// An item changing hands: the mark the 3 handover calls raise, what a stack
// that changes hands gets, a trader's loose item, SpawnDupe's copies and the
// spawn container scope. Private to this folder.
namespace SpawnCondition
{
	// What a handover in progress on this thread moves: the item, who gets it
	// and whether it comes straight from the world. Lives only for that one
	// call to TESObjectREFR::AddInventoryItem.
	struct Handing
	{
		const RE::TESBoundObject* object{ nullptr };
		const RE::TESObjectREFR*  receiver{ nullptr };
		bool                      fromWorld{ false };
	};

	// The handover moving a_object into a_list, or nothing. AddInventoryItem
	// adds to its receiver's own inventory, so the list names the receiver.
	[[nodiscard]] const Handing* HandingOver(const RE::TESBoundObject* a_object, const RE::BGSInventoryList* a_list);

	// What a stack a handover names gets, written before the original
	// AddStack. Returns the aim of a first roll, so the caller splits a stack
	// of several after the add, or nothing. a_stack is private, see Private
	// in Guards.h, and the switch is on.
	std::optional<Aim> HandOver(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack,
		const Handing& a_handing);

	// Whether a_list is the barter screen's copy of a trader's goods.
	[[nodiscard]] bool BarterCopy(const RE::BGSInventoryList* a_list);

	// A loose item the trader's faction owns, copied into the barter copy
	// a_list: 1 roll for the item in the world, whatever the count, shown and
	// sold alike. False when a_stack is no such item, which is then a
	// trader's copy for the caller to leave alone.
	bool LooseItem(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack);

	// Whether this thread is in SpawnDupe's copy of a character's stacks.
	// Each copy passes the source's own extra data list, so nothing may be
	// written to it.
	[[nodiscard]] bool UnderSpawnDupe();

	// Keeps the pickup mark down on this thread while alive. The game moves
	// an item whose own placed reference names a spawn container from the
	// world into that container through the pickup road, and that item is
	// born there, not picked up.
	class ScopedSpawnContainer
	{
	public:
		ScopedSpawnContainer();

	private:
		Scoped<bool> _mark;
	};

	// Patches the 3 handover calls and SpawnDupe's copy, and says so in the
	// log.
	void InstallHandover();
}
