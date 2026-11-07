#include "Condition/CraftingPerks/Description.h"

#include "Condition/CraftingPerks/CraftingPerks.h"
#include "Condition/CraftingPerks/Ladder.h"
#include "Condition/CraftingPerks/Perks.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace CraftingPerks
{
	namespace
	{
		// The 2 places a perk's words are read for the screen: the level up
		// chart and the Pip-Boy's perks page. The Pip-Boy shrinks the letters
		// of a long description to fit, and UI/PerkChart.cpp grows the chart's
		// field, 2 lines high on its own, to every line.
		constexpr CallPatch::CallSite DESCRIPTION_SITES[]{
			{ CallPatch::AE{ 2206559, 0x434 }, CallPatch::OG{ 1471750, 0x382 }, "perk chart" },
			{ CallPatch::AE{ 2225718, 0x36A }, CallPatch::OG{ 1034674, 0x29A }, "Pip-Boy perks page" },
		};

		using GetDescription_t = void (*)(RE::TESDescription*, RE::BSString&, const RE::TESForm*);

		std::array<CallPatch::Link<GetDescription_t>, std::size(DESCRIPTION_SITES)> g_links;

		// Whether the bench repairs, see SetBench. Set once at install.
		bool (*g_benchRepairs)() = nullptr;

		// Scrapper01 in Fallout4.esm, by its ID, the same on OG and AE. Its
		// next ranks are walked from it.
		constexpr RE::TESFormID SCRAPPER = 0x065E65;

		[[nodiscard]] const Told* Ours(const Perks& a_perks, const RE::TESDescription* a_description)
		{
			const auto found = std::find_if(a_perks.told.begin(), a_perks.told.end(),
				[a_description](const Told& a_told) { return a_told.description == a_description; });
			return found != a_perks.told.end() ? &*found : nullptr;
		}

		// The Scrapper rank a_description belongs to, from 1, or 0 for none.
		[[nodiscard]] std::uint32_t ScrapperRank(const Perks& a_perks, const RE::TESDescription* a_description)
		{
			const auto found = std::find_if(a_perks.scrapper.begin(), a_perks.scrapper.end(),
				[a_description](const RE::BGSPerk* a_perk) { return static_cast<const RE::TESDescription*>(a_perk) == a_description; });
			return found != a_perks.scrapper.end() ? static_cast<std::uint32_t>(found - a_perks.scrapper.begin()) + 1 : 0;
		}

		void Join(std::string& a_said, const std::string& a_line)
		{
			if (!a_said.empty()) {
				a_said += " ";
			}
			a_said += a_line;
		}

		// Adds the plugin's line after the words already there, so a
		// description rewritten by another plugin or read in another language
		// comes through. Joined with a space, never a line break: the level up
		// chart keeps only the first 2 pieces split on newlines and would drop
		// the whole sentence. One per site, and only while the bench repairs.
		// Scrapper's line shows only while spare repair is on too.
		template <std::size_t I>
		void DescriptionHk(RE::TESDescription* a_description, RE::BSString& a_out, const RE::TESForm* a_form)
		{
			g_links[I](a_description, a_out, a_form);
			const auto  bench = g_benchRepairs && g_benchRepairs();
			const auto  perks = g_links[I].Live() && bench ? Current() : nullptr;
			const auto* ours = perks ? Ours(*perks, a_description) : nullptr;
			const auto  scrapper = perks && Settings::bSpareRepair.GetValue() ? ScrapperRank(*perks, a_description) : 0U;
			if (!ours && scrapper == 0) {
				return;
			}

			const auto* already = a_out.c_str();
			std::string said = already ? already : "";
			if (ours) {
				Join(said, Text::PerkDiscount(ours->percent, ours->weapons, ours->armor));
			}
			if (scrapper > 0) {
				Join(said, Text::PerkScrapper());
			}
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
				const auto* perk = ours ? ours->perk : perks->scrapper[scrapper - 1];
				TraceLog::Once("menu", "{:s} rank {:d} [{:08X}] reads \"{:s}\"",
					perk ? RE::TESFullName::GetFullName(*perk) : "a perk"sv, ours ? ours->rank : scrapper,
					perk ? perk->formID : 0U, shown);
			}
		}
	}

	void InstallDescriptions()
	{
		const auto hooks = CallPatch::PerSite<std::size(DESCRIPTION_SITES)>([]<std::size_t I>() { return &DescriptionHk<I>; });
		CallPatch::PatchAll(DESCRIPTION_SITES, RE::ID::TESDescription::GetDescription, hooks, g_links,
			"Crafting perks say what they do to a repair");
	}

	void SetBench(bool (*a_repairs)())
	{
		g_benchRepairs = a_repairs;
	}

	void TellRanks(Perks& a_perks, std::span<const RE::BGSPerk* const> a_first)
	{
		// Walked from the first rank, and each next rank asks a higher level,
		// so the count is the rank. The line names what the perk's mods go on,
		// weapons, armor or both, since Science unlocks laser receivers and
		// lead lined armor alike.
		for (const auto* first : a_first) {
			const auto    ranks = Ranks(first);
			const auto    gates = GatesOf(a_perks, first);
			const auto*   walk = first;
			std::uint32_t at = 0;
			do {
				at++;
				a_perks.told.push_back({ static_cast<const RE::TESDescription*>(walk), Discount(at, ranks), walk, at, gates.weapons > 0, gates.armor > 0 });
				walk = walk->nextPerk;
			} while (walk && walk != first && at < LADDER_LIMIT);
		}
	}

	void TellScrapper(Perks& a_perks)
	{
		const auto* first = g_dataHandler ? g_dataHandler->LookupForm<RE::BGSPerk>(SCRAPPER, "Fallout4.esm") : nullptr;
		const auto* walk = first;
		while (walk && a_perks.scrapper.size() < LADDER_LIMIT) {
			a_perks.scrapper.push_back(walk);
			walk = walk->nextPerk == first ? nullptr : walk->nextPerk;
		}
	}
}
