#include "Condition/CraftingPerks/Ladder.h"

namespace CraftingPerks
{
	const RE::BGSPerk* FirstRank(const RE::BGSPerk* a_perk)
	{
		if (!a_perk) {
			return nullptr;
		}

		const auto* walk = a_perk->nextPerk;
		for (std::uint32_t step = 0; walk && step < LADDER_LIMIT; step++) {
			if (walk->data.level <= a_perk->data.level) {
				return walk;
			}
			walk = walk->nextPerk;
		}

		// A perk with no ring is a single rank on its own.
		return a_perk;
	}

	std::uint32_t Ranks(const RE::BGSPerk* a_first)
	{
		if (!a_first) {
			return 0;
		}

		std::uint32_t count = 0;
		const auto*   walk = a_first;
		do {
			count++;
			walk = walk->nextPerk;
		} while (walk && walk != a_first && count < LADDER_LIMIT);
		return count;
	}

	std::uint32_t RankHeld(const RE::BGSPerk* a_first)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!a_first || !player) {
			return 0;
		}

		std::uint32_t held = 0;
		std::uint32_t at = 0;
		const auto*   walk = a_first;
		do {
			at++;
			if (player->GetPerkRank(const_cast<RE::BGSPerk*>(walk)) != 0) {
				held = at;
			}
			walk = walk->nextPerk;
		} while (walk && walk != a_first && at < LADDER_LIMIT);
		return held;
	}
}
