#pragma once

#include "Core/Plugin.h"

#include <cstdint>

// A perk's ranks, linked in a ring. Private to this folder.
namespace CraftingPerks
{
	// Every walk of the ring stops here, in case a broken ring never closes. No
	// perk has anywhere near this many ranks.
	inline constexpr std::uint32_t LADDER_LIMIT = 64;

	// The first rank of the perk. Ranks are linked in a ring by next, Gun Nut 4
	// points back to Gun Nut 1, so the first rank is found by the level each
	// asks for, which rises round the ring and only drops where it wraps. The
	// game does the same in GetPlayerPerkLadderRank.
	[[nodiscard]] const RE::BGSPerk* FirstRank(const RE::BGSPerk* a_perk);

	// How many ranks a perk has, counted round the ring rather than read from
	// the record. Rifleman's last rank claims 10, and the ring is what the game
	// counts.
	[[nodiscard]] std::uint32_t Ranks(const RE::BGSPerk* a_first);

	// How many ranks of a perk the player has. Each rank taken is its own perk
	// record and the lower ones stay, so the rank is the last one in the ring
	// they have.
	[[nodiscard]] std::uint32_t RankHeld(const RE::BGSPerk* a_first);
}
