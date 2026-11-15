#include "Condition/CraftingPerks/Description.h"

#include "Condition/CraftingPerks/CraftingPerks.h"
#include "Condition/CraftingPerks/Ladder.h"
#include "Condition/CraftingPerks/Perks.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/ScriptEvents.h"
#include "Core/Settings.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog/TraceLog.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

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

		// DESCRIPTION_SITES[1], the Pip-Boy perks page, see RetellPipboy.
		constexpr std::size_t PIPBOY = 1;

		using GetDescription_t = void (*)(RE::TESDescription*, RE::BSString&, const RE::TESForm*);

		std::array<CallPatch::Link<GetDescription_t>, std::size(DESCRIPTION_SITES)> g_links;

		// Whether the bench repairs, see SetBench. Set once at install.
		bool (*g_benchRepairs)() = nullptr;

		// The repair lock's rule, see SetLock. Set once at install.
		std::optional<std::uint32_t> (*g_rankLock)(std::uint32_t, std::uint32_t, std::uint32_t) = nullptr;

		// Scrapper01 in Fallout4.esm, by its ID, the same on OG and AE. Its
		// next ranks are walked from it.
		constexpr RE::TESFormID SCRAPPER = 0x065E65;

		// -------------------------------------------------------------------
		// The words NEC adds
		// -------------------------------------------------------------------

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

		// NEC's lines for a_description, or nothing: the sentence PerkRepairs
		// forms, then Scrapper's line while spare repair is on. Worked out at
		// each ask, so they follow the settings.
		[[nodiscard]] std::string Added(const Perks& a_perks, const RE::TESDescription* a_description)
		{
			std::string added;
			if (const auto* ours = Ours(a_perks, a_description)) {
				const auto off = Discount(ours->rank, ours->ranks);
				const auto from = g_rankLock ? g_rankLock(ours->rank, ours->ranks, ours->highest) : std::nullopt;
				if (off > 0 || from) {
					Join(added, Text::PerkRepairs(off, ours->weapons, ours->armor, from));
				}
			}
			if (Settings::bSpareRepair.GetValue() && ScrapperRank(a_perks, a_description) > 0) {
				Join(added, Text::PerkScrapper());
			}
			return added;
		}

		// A rank NEC knows, from 1, with the words the Pip-Boy perks page was
		// handed before NEC (base) and what NEC handed on (handed).
		struct Kept
		{
			const RE::BGSPerk* perk{ nullptr };
			std::uint32_t      rank{ 0 };
			std::string        base{};
			std::string        handed{};
		};

		// The rank a_description belongs to, settings aside: a crafting perk's
		// first, else Scrapper's, else none. The hook adds its words.
		[[nodiscard]] std::optional<Kept> Whose(const Perks& a_perks, const RE::TESDescription* a_description)
		{
			if (const auto* ours = Ours(a_perks, a_description)) {
				return Kept{ ours->perk, ours->rank };
			}
			const auto rank = ScrapperRank(a_perks, a_description);
			return rank > 0 ? std::optional{ Kept{ a_perks.scrapper[rank - 1], rank } } : std::nullopt;
		}

		// a_text for the trace with its line breaks spelled out, since the
		// level up chart drops what follows the second.
		[[nodiscard]] std::string Spelled(std::string_view a_text)
		{
			std::string shown;
			for (const auto c : a_text) {
				shown += c == '\n' ? "\\n" : c == '\r' ? "\\r" : std::string(1, c);
			}
			return shown;
		}

		// -------------------------------------------------------------------
		// The hook
		// -------------------------------------------------------------------

		// Every rank NEC knows, by its description. Written by the hook at
		// Populate on the load thread, read by RetellPipboy on the main thread.
		std::mutex                                          g_keptLock;
		std::unordered_map<const RE::TESDescription*, Kept> g_kept;

		// Adds the plugin's lines after the words already there, so a
		// description rewritten by another plugin or read in another language
		// comes through. Joined with a space, never a line break: the level up
		// chart keeps only the first 2 pieces split on newlines and would drop
		// the whole sentence. One per site, and only while the bench repairs.
		// The Pip-Boy's site keeps what it gave each rank NEC knows, even with
		// no line added, so a later switch can add one. Other ranks are dropped.
		template <std::size_t I>
		void DescriptionHk(RE::TESDescription* a_description, RE::BSString& a_out, const RE::TESForm* a_form)
		{
			g_links[I](a_description, a_out, a_form);
			const auto bench = g_benchRepairs && g_benchRepairs();
			const auto perks = g_links[I].Live() && bench ? Current() : nullptr;
			auto       kept = perks ? Whose(*perks, a_description) : std::nullopt;
			if (!kept) {
				if constexpr (I == PIPBOY) {
					const std::scoped_lock l{ g_keptLock };
					g_kept.erase(a_description);
				}
				return;
			}

			const auto* already = a_out.c_str();
			kept->base = already ? already : "";
			kept->handed = kept->base;
			const auto added = Added(*perks, a_description);
			if (!added.empty()) {
				Join(kept->handed, added);
				a_out.Set(kept->handed.c_str(), 0);
			}

			// A screen asks at every draw, so the trace says each once.
			if (!added.empty() && TraceLog::IsOpen()) {
				const auto* perk = kept->perk;
				TraceLog::Once("menu", "{:s} rank {:d} [{:08X}] reads \"{:s}\"",
					perk ? RE::TESFullName::GetFullName(*perk) : "a perk"sv, kept->rank,
					perk ? perk->formID : 0U, Spelled(kept->handed));
			}
			if constexpr (I == PIPBOY) {
				const std::scoped_lock l{ g_keptLock };
				g_kept.insert_or_assign(a_description, std::move(*kept));
			}
		}

		// -------------------------------------------------------------------
		// Telling the Pip-Boy again
		// -------------------------------------------------------------------

		// 1 kept rank on its way back into the Pip-Boy's tree.
		struct Retold
		{
			Kept        kept;
			std::string now{};    // its base and NEC's lines as they read now
			std::string gives{};  // the page's whole text once rewritten
			bool        met{ false };
			bool        rewritten{ false };
			bool        noText{ false };   // the rank holds no text value NEC can rewrite
			bool        changed{ false };  // another mod changed its text after NEC
		};

		using Retelling = std::unordered_map<const RE::TESDescription*, Retold>;

		// Puts a_retold's words where NEC's were in a_rank's Description, so
		// another mod's words around them stay, or marks why it cannot.
		void Retell(RE::PipboyObject& a_rank, const RE::BSFixedString& a_key, Retold& a_retold)
		{
			using StringValue = RE::PipboyPrimitiveValue<RE::BSString>;
			auto* value = a_rank.GetMember<RE::PipboyValue*>(a_key);
			if (!value || *reinterpret_cast<const std::uintptr_t*>(value) != StringValue::VTABLE[0].address()) {
				a_retold.noText = true;
				return;
			}
			const auto*            text = static_cast<const StringValue*>(value);
			const auto*            letters = text->m_value.c_str();
			const std::string_view had{ letters ? letters : "" };
			const auto&            handed = a_retold.kept.handed;
			// Empty handed words match only an empty text.
			const auto at = handed.empty() && !had.empty() ? std::string_view::npos : had.find(handed);
			if (at == std::string_view::npos) {
				a_retold.changed = true;
				return;
			}

			a_retold.met = true;
			std::string next{ had.substr(0, at) };
			next += a_retold.now;
			next += had.substr(at + handed.size());
			if (next != had) {
				// The engine's AddMember frees the old value and marks the tree
				// changed. The new value takes the engine's heap.
				a_rank.AddMember(&a_key, new StringValue(next.c_str(), &a_rank));
				a_retold.rewritten = true;
				a_retold.gives = std::move(next);
			}
		}

		// Walks the tree as Populate builds it, under the group's lock: chain
		// i is an object whose "Perks" array holds 1 object per rank, walked
		// from perkChainRoots[i] by next. a_walked gets each rank met, once.
		void Walk(RE::PipboyPerksData& a_data, Retelling& a_ranks, std::vector<const Retold*>& a_walked)
		{
			// Made before the lock, since a new key takes the string pool's.
			const RE::BSFixedString perksKey{ "Perks" };
			const RE::BSFixedString descriptionKey{ "Description" };
			a_data.LockDataGroup();
			auto*      chains = a_data.perkChains;
			const auto count = chains ? std::min<std::size_t>(chains->elements.size(), a_data.perkChainRoots.size()) : 0;
			for (std::uint32_t i = 0; i < count; i++) {
				auto* chain = chains->elements[i];
				if (!chain || chain->GetType() != RE::PipboyValue::kObject) {
					continue;
				}
				auto* list = static_cast<RE::PipboyObject*>(chain)->GetMember<RE::PipboyValue*>(perksKey);
				if (!list || list->GetType() != RE::PipboyValue::kArray) {
					continue;
				}
				const auto* perk = a_data.perkChainRoots[i];
				auto*       rankArray = static_cast<RE::PipboyArray*>(list);
				for (auto* element : rankArray->elements) {
					const auto found = perk ? a_ranks.find(static_cast<const RE::TESDescription*>(perk)) : a_ranks.end();
					if (found != a_ranks.end() && element && element->GetType() == RE::PipboyValue::kObject) {
						Retell(*static_cast<RE::PipboyObject*>(element), descriptionKey, found->second);
						if (std::ranges::find(a_walked, &found->second) == a_walked.end()) {
							a_walked.push_back(&found->second);
						}
					}
					perk = perk && perk->nextPerk ? perk->nextPerk : perk;
				}
			}
			a_data.UnlockDataGroup();
		}

		// The retell's trace, in walk order so it reads the same on every run,
		// and in words of its own so it never reads as the hook's lines.
		void Say(const std::vector<const Retold*>& a_walked, std::size_t a_kept)
		{
			std::size_t rewritten = 0;
			for (const auto* retold : a_walked) {
				const auto* perk = retold->kept.perk;
				const auto  name = perk ? RE::TESFullName::GetFullName(*perk) : "a perk"sv;
				if (retold->rewritten) {
					rewritten++;
					TraceLog::Line("menu", "Pip-Boy perks page now gives {:s} rank {:d} [{:08X}] \"{:s}\"",
						name, retold->kept.rank, perk ? perk->formID : 0U, Spelled(retold->gives));
				}
				if (retold->noText) {
					TraceLog::Line("menu", "Pip-Boy perks page keeps {:s} rank {:d}'s text, since the rank holds no text value NEC can rewrite",
						name, retold->kept.rank);
				}
				if (retold->changed) {
					TraceLog::Line("menu", "Pip-Boy perks page keeps {:s} rank {:d}'s text, which another mod changed after NEC",
						name, retold->kept.rank);
				}
			}
			TraceLog::Line("menu", "Pip-Boy perks page reads {:d} of NEC's {:d} ranks again after a setting changed",
				rewritten, a_kept);
		}
	}

	void InstallDescriptions()
	{
		const auto hooks = CallPatch::PerSite<std::size(DESCRIPTION_SITES)>([]<std::size_t I>() { return &DescriptionHk<I>; });
		CallPatch::PatchAll(DESCRIPTION_SITES, RE::ID::TESDescription::GetDescription, hooks, g_links,
			"Crafting perks say what they do to a repair");
	}

	void ForgetPipboy()
	{
		const std::scoped_lock l{ g_keptLock };
		g_kept.clear();
	}

	void SetBench(bool (*a_repairs)())
	{
		g_benchRepairs = a_repairs;
	}

	void SetLock(std::optional<std::uint32_t> (*a_opensFrom)(std::uint32_t a_rank, std::uint32_t a_ranks, std::uint32_t a_highest))
	{
		g_rankLock = a_opensFrom;
	}

	void RetellPipboy()
	{
		// Populate fills the tree with no group lock, after the load's events
		// end, at its rebuild and a loading screen's end. This runs only as a
		// task at the next menu frame after an MCM change, which no loading
		// screen overlaps, and returns while Loading() says a load is under way.
		const auto perks = Current();
		auto*      manager = RE::PipboyDataManager::GetSingleton();
		if (!g_links[PIPBOY].Live() || ScriptEvents::Loading() || !perks || !manager) {
			return;
		}

		// Worked out before the lock. The site's callee is never called again,
		// so no other mod's code runs outside the game's own Populate.
		Retelling ranks;
		{
			const std::scoped_lock l{ g_keptLock };
			for (const auto& [description, kept] : g_kept) {
				ranks.emplace(description, Retold{ kept });
			}
		}
		const auto bench = g_benchRepairs && g_benchRepairs();
		for (auto& [description, retold] : ranks) {
			retold.now = retold.kept.base;
			if (const auto added = bench ? Added(*perks, description) : std::string{}; !added.empty()) {
				Join(retold.now, added);
			}
		}

		std::vector<const Retold*> walked;
		Walk(manager->perksData, ranks, walked);

		// So a 2nd change finds what this one handed on.
		{
			const std::scoped_lock l{ g_keptLock };
			for (const auto& [description, retold] : ranks) {
				if (const auto kept = g_kept.find(description); retold.met && kept != g_kept.end()) {
					kept->second.handed = retold.now;
				}
			}
		}
		if (TraceLog::IsOpen()) {
			Say(walked, ranks.size());
		}
	}

	void TellRanks(Perks& a_perks, std::span<const RE::BGSPerk* const> a_first)
	{
		// Walked from the first rank, and each next rank asks a higher level,
		// so the count is the rank. The line names what the perk's mods go on,
		// weapons, armor or both, since Science unlocks laser receivers and
		// lead lined armor alike. The repair lock's condition counts from the
		// highest rank the recipes ask, 4 of Gun Nut in vanilla.
		for (const auto* first : a_first) {
			const auto    ranks = Ranks(first);
			const auto    gates = GatesOf(a_perks, first);
			const auto    highest = std::min(AsksOf(a_perks, first), ranks);
			const auto*   walk = first;
			std::uint32_t at = 0;
			do {
				at++;
				a_perks.told.push_back({ static_cast<const RE::TESDescription*>(walk), ranks, walk, at, gates.weapons > 0, gates.armor > 0, highest });
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
