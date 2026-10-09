#pragma once

#include "Core/Plugin.h"

// Who a stack is going to, and which of them get it at full condition.
// Private to this folder.
namespace SpawnCondition
{
	// Whether a stack is going to the player. The player's own are listed one
	// by one in the trace log and everybody else's counted, since a save
	// loading hands thousands to the characters and containers around the
	// player at once.
	[[nodiscard]] bool Players(const RE::BGSInventoryList* a_list);

	// The record that makes the owner of a_list essential, a companion's for
	// example, or nothing. Read from the record, since the game marks the
	// character only once they are up and about, long after their gear
	// arrives. A leveled character is judged by the record placed in the
	// world, as the game reads it.
	[[nodiscard]] const RE::TESActorBase* Essential(const RE::BGSInventoryList* a_list);

	// The outfit on the player's own record that a_extra comes from, or
	// nothing. A new game dresses the player from it, in the Vault 111
	// Jumpsuit and Pip-Boy. The game tags each piece with its outfit's ID
	// before the piece goes in, and the tag follows it into any inventory, so
	// loot from anyone else carries their outfit or none. Read under the
	// inventory lock, as Showpiece reads.
	[[nodiscard]] const RE::BGSOutfit* PlayersOutfit(const RE::ExtraDataList& a_extra);

	// Whether a stack is a showpiece: a legendary weapon or piece of armor
	// going into a chest, not into a character's hands. A trader's showpiece,
	// Big Boy at Arturo's for one, is placed by a quest script into a chest
	// that never restocks, so it passes here once, as plain loot. A corpse's
	// legendary goes to the dying character first, so it stays loot. Read
	// under the inventory lock. The extra data takes its own lock after it, as
	// the script guards do.
	[[nodiscard]] bool Showpiece(const RE::BGSInventoryList* a_list, RE::ExtraDataList& a_extra);
}
