#include "Condition/CraftingPerks/Ladder.h"

#include "Core/TraceLog.h"

#include <utility>

namespace CraftingPerks
{
	namespace
	{
		// Whether the player holds one rank, as the HasPerk condition the
		// recipes use answers it. A skill mod can answer that condition from its
		// skills through another DLL mod that writes its own function into the
		// game's table, and GetPerkRank would then say no and price every repair
		// at 2x. The game reads the table at every check, and so does this, so
		// a function written there later still counts.
		[[nodiscard]] bool Holds(RE::PlayerCharacter& a_player, const RE::BGSPerk& a_rank)
		{
			auto* const perk = const_cast<RE::BGSPerk*>(&a_rank);
			const auto  entry = std::to_underlying(RE::SCRIPT_OUTPUT::kScript_HasPerk);
			auto* const condition = RE::SCRIPT_FUNCTION::GetScriptFunctions()[entry].conditionFunction;
			if (!condition) {
				return a_player.GetPerkRank(perk) != 0;
			}

			RE::ConditionCheckParams params;
			params.actionRef = &a_player;
			float result = 0.0F;
			condition(params, perk, nullptr, result);
			const bool held = result != 0.0F;

			if (TraceLog::IsOpen() && held != (a_player.GetPerkRank(perk) != 0)) {
				TraceLog::First("menu", "The HasPerk condition says the player {:s} {:s} [{:08X}] while the player's own perks say the opposite, and NEC goes by the condition",
					held ? "has" : "does not have", RE::TESFullName::GetFullName(a_rank), a_rank.formID);
			}
			return held;
		}
	}

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
			if (Holds(*player, *walk)) {
				held = at;
			}
			walk = walk->nextPerk;
		} while (walk && walk != a_first && at < LADDER_LIMIT);
		return held;
	}
}
