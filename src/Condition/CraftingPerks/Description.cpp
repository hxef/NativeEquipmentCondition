#include "Condition/CraftingPerks/Description.h"

#include "Condition/CraftingPerks/CraftingPerks.h"
#include "Condition/CraftingPerks/Ladder.h"
#include "Core/CallPatch.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace CraftingPerks
{
	namespace
	{
		// One rank's own description, what it takes off a repair, and what its
		// mods go on. perk and rank name it in the trace.
		struct Told
		{
			const RE::TESDescription* description{ nullptr };
			std::uint32_t             percent{ 0 };
			const RE::BGSPerk*        perk{ nullptr };
			std::uint32_t             rank{ 0 };
			bool                      weapons{ false };
			bool                      armor{ false };
		};

		// Every rank of every perk that can price a repair, 15 in vanilla, so a
		// plain loop is fast enough.
		std::vector<Told> g_told;

		REL::Relocation<void (*)(RE::TESDescription*, RE::BSString&, const RE::TESForm*)> _GetDescription;

		// The 2 places a perk's words are read for the screen: the level up
		// chart and the Pip-Boy's perks page.
		constexpr CallPatch::CallSite DESCRIPTION_SITES[]{
			{ 2206559, 0x434, "perk chart" },
			{ 2225718, 0x36A, "Pip-Boy perks page" },
		};

		[[nodiscard]] const Told* Ours(const RE::TESDescription* a_description)
		{
			const auto found = std::find_if(g_told.begin(), g_told.end(),
				[a_description](const Told& a_told) { return a_told.description == a_description; });
			return found != g_told.end() ? &*found : nullptr;
		}

		// Adds the plugin's line after the game's own words, so a description
		// rewritten by another plugin or read in another language comes
		// through. Joined with a space, never a line break: the level up chart
		// keeps only the first 2 pieces split on newlines and would drop the
		// whole sentence.
		void DescriptionHk(RE::TESDescription* a_description, RE::BSString& a_out, const RE::TESForm* a_form)
		{
			_GetDescription(a_description, a_out, a_form);
			const auto* ours = Ours(a_description);
			if (!ours) {
				return;
			}

			const auto* already = a_out.c_str();
			std::string said = already ? already : "";
			if (!said.empty()) {
				said += " ";
			}
			said += Text::PerkDiscount(ours->percent, ours->weapons, ours->armor);
			a_out.Set(said.c_str(), 0);

			// A screen asks for a description every time it draws, so the trace
			// says each once. Line breaks are spelled out, since the level up
			// chart drops what follows the second.
			if (TraceLog::IsOpen()) {
				std::string shown;
				for (const auto c : said) {
					if (c == '\n') {
						shown += "\\n";
					} else if (c == '\r') {
						shown += "\\r";
					} else {
						shown += c;
					}
				}
				TraceLog::Once("menu", "{:s} rank {:d} [{:08X}] reads \"{:s}\"",
					ours->perk ? RE::TESFullName::GetFullName(*ours->perk) : "a perk"sv, ours->rank,
					ours->perk ? ours->perk->formID : 0U, shown);
			}
		}
	}

	void InstallDescriptions()
	{
		_GetDescription = RE::ID::TESDescription::GetDescription;

		const auto hooks = CallPatch::Repeat<std::size(DESCRIPTION_SITES)>(
			reinterpret_cast<std::uintptr_t>(DescriptionHk));
		CallPatch::PatchAll(DESCRIPTION_SITES, RE::ID::TESDescription::GetDescription, hooks,
			"Crafting perks say what they do to a repair");
	}

	void TellRanks(std::span<const RE::BGSPerk* const> a_first)
	{
		// Walked from the first rank, and each next rank asks a higher level,
		// so the count is the rank.
		for (const auto* first : a_first) {
			const auto    ranks = Ranks(first);
			const auto    weapons = PricesWeapons(first);
			const auto    armor = PricesArmor(first);
			const auto*   walk = first;
			std::uint32_t at = 0;
			do {
				at++;
				g_told.push_back({ static_cast<const RE::TESDescription*>(walk), Discount(at, ranks), walk, at, weapons, armor });
				walk = walk->nextPerk;
			} while (walk && walk != first && at < LADDER_LIMIT);
		}
	}

	void ForgetTold()
	{
		g_told.clear();
	}
}
