#include "Condition/CraftingPerks/CraftingPerks.h"

#include "Condition/Condition.h"
#include "Condition/CraftingPerks/Description.h"
#include "Condition/CraftingPerks/Ladder.h"
#include "Condition/CraftingPerks/Recipes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>

namespace CraftingPerks
{
	namespace
	{
		// One perk and its count: recipes when choosing the perk for a kind of
		// weapon, component units when choosing it for one item.
		struct Tally
		{
			const RE::BGSPerk* perk{ nullptr };
			std::uint32_t      count{ 0 };
		};

		// The perk behind most weapon mods of each kind, for weapons whose
		// parts name none. Indexed by the game's weapon type byte, hand to hand
		// at 0 to mine at 11. Filled by Load and emptied by Unload while
		// nothing reads it, so no lock.
		std::array<const RE::BGSPerk*, KINDS> g_kinds{};

		// How many weapon mod recipes each perk unlocks across the load order.
		// Breaks the tie for a weapon that names 2 perks equally, like the Fat
		// Man, half Gun Nut and half Science: Gun Nut unlocks 370 and Science
		// 240, so it goes under Gun Nut, where vanilla puts it.
		std::unordered_map<const RE::BGSPerk*, std::uint32_t> g_weight;

		// Every recipe that names a perk, and the perks it names. The game
		// states it on the recipe, so nothing here is guessed.
		std::unordered_map<const RE::BGSConstructibleObject*, std::vector<const RE::BGSPerk*>> g_byRecipe;

		// How many recipes one perk unlocks, looked up without adding an entry.
		[[nodiscard]] std::uint32_t Weight(const RE::BGSPerk* a_perk)
		{
			const auto found = a_perk ? g_weight.find(a_perk) : g_weight.end();
			return found != g_weight.end() ? found->second : 0;
		}

		// Adds a_count to a_perk's entry in a tally.
		void Note(std::vector<Tally>& a_tally, const RE::BGSPerk* a_perk, std::uint32_t a_count)
		{
			const auto found = std::find_if(a_tally.begin(), a_tally.end(),
				[a_perk](const Tally& a_seen) { return a_seen.perk == a_perk; });
			if (found != a_tally.end()) {
				found->count += a_count;
			} else {
				a_tally.push_back({ a_perk, a_count });
			}
		}
	}

	Standing OfRecipe(const RE::BGSConstructibleObject& a_recipe)
	{
		Standing out;
		const auto found = g_byRecipe.find(&a_recipe);
		if (found == g_byRecipe.end()) {
			return out;
		}

		// A part can need 2 perks, a recon scope Gun Nut for the mount and
		// Science for the glass, and about 130 recipes do. The one the player
		// has taken furthest counts, judged by what it does to the price and
		// not by rank number. 2 that come out equal cost the same, so there is
		// no tie break.
		auto best = UNSKILLED_MULTIPLE;
		for (const auto* ladder : found->second) {
			Standing standing;
			standing.perk = ladder;
			standing.ranks = Ranks(ladder);
			standing.rank = RankHeld(ladder);

			const auto multiple = Multiple(standing);
			if (!out.perk || multiple < best) {
				best = multiple;
				out = standing;
			}
		}
		return out;
	}

	float Multiple(const Standing& a_standing)
	{
		return UNSKILLED_MULTIPLE - (UNSKILLED_MULTIPLE - SKILLED_MULTIPLE) * a_standing.Share();
	}

	std::uint32_t Discount(std::uint32_t a_rank, std::uint32_t a_ranks)
	{
		if (a_ranks == 0) {
			return 0;
		}

		Standing standing;
		standing.rank = a_rank;
		standing.ranks = a_ranks;
		const auto saved = (UNSKILLED_MULTIPLE - Multiple(standing)) / UNSKILLED_MULTIPLE;
		return static_cast<std::uint32_t>(std::lround(saved * 100.0F));
	}

	std::string Standing::Name() const
	{
		return perk ? std::string{ RE::TESFullName::GetFullName(*perk) } : std::string{ "nothing" };
	}

	void Install()
	{
		InstallDescriptions();
	}

	void Unload()
	{
		g_kinds.fill(nullptr);
		g_weight.clear();
		g_byRecipe.clear();
		ForgetTold();
	}

	void Load()
	{
		Unload();
		if (!g_dataHandler) {
			return;
		}

		std::unordered_set<const RE::TESForm*> perks;
		for (const auto* perk : g_dataHandler->GetFormArray<RE::BGSPerk>()) {
			if (perk) {
				perks.insert(perk);
			}
		}

		// Which kinds of weapon offer each slot, a bit per kind. A slot no
		// weapon offers never appears, which keeps armor out of the count
		// without naming it.
		std::unordered_map<const RE::BGSKeyword*, std::uint32_t> offering;
		std::uint32_t                                            weapons = 0;
		for (const auto* weapon : g_dataHandler->GetFormArray<RE::TESObjectWEAP>()) {
			if (!weapon || !Condition::WearsOut(*weapon)) {
				continue;
			}
			weapons++;

			const auto  bit = 1U << KindOf(*weapon, nullptr);
			const auto& parents = weapon->attachParents;
			for (std::uint32_t i = 0; parents.array && i < parents.size; i++) {
				if (const auto* point = PointOf(parents.array[i].keywordIndex)) {
					offering[point] |= bit;
				}
			}
		}

		// Which kinds of weapon a recipe's mod could go on. A recipe building a
		// list of mods counts everything any of them can go on.
		const auto reached = [&offering](const RE::TESForm* a_built) -> std::uint32_t {
			if (!a_built || !a_built->Is(RE::ENUM_FORM_ID::kOMOD)) {
				return 0;
			}

			const auto& mod = *static_cast<const RE::BGSMod::Attachment::Mod*>(a_built);
			if (mod.targetFormType.get() != RE::ENUM_FORM_ID::kWEAP) {
				return 0;
			}

			const auto* point = PointOf(mod.attachPoint.keywordIndex);
			const auto  found = point ? offering.find(point) : offering.end();
			return found != offering.end() ? found->second : 0;
		};

		// Every recipe that needs a perk counts that perk once for every kind
		// of weapon the mod it builds could go on.
		std::array<std::vector<Tally>, KINDS> byKind;
		std::vector<const RE::BGSPerk*>       ladders;
		std::vector<const RE::BGSPerk*>       alone;
		std::uint32_t                         gated = 0;

		for (const auto* recipe : g_dataHandler->GetFormArray<RE::BGSConstructibleObject>()) {
			const auto* built = recipe ? recipe->createdItem : nullptr;
			if (!built) {
				continue;
			}

			LaddersOf(*recipe, perks, ladders);
			if (ladders.empty()) {
				continue;
			}

			// Kept whatever it builds, a weapon mod or a suit of armor, since
			// the bench prices whatever recipe is in front of it. The count
			// below is stricter.
			g_byRecipe.emplace(recipe, ladders);

			// The same 2 forms Materials::Load reads: the item itself, or a
			// list of items.
			std::uint32_t kinds = 0;
			if (built->Is(RE::ENUM_FORM_ID::kFLST)) {
				for (const auto* listed : static_cast<const RE::BGSListForm*>(built)->arrayOfForms) {
					kinds |= reached(listed);
				}
			} else {
				kinds = reached(built);
			}
			if (kinds == 0) {
				continue;
			}

			gated++;
			for (const auto* ladder : ladders) {
				g_weight[ladder]++;
				for (std::size_t kind = 0; kind < KINDS; kind++) {
					if (kinds & (1U << kind)) {
						Note(byKind[kind], ladder, 1);
					}
				}
			}

			// A perk that is the only one a part needs can price a gun on its
			// own. The 4 combat perks never are in vanilla: every Handmade
			// Rifle receiver that needs Commando names Gun Nut beside it.
			if (ladders.size() == 1 &&
				std::find(alone.begin(), alone.end(), ladders.front()) == alone.end()) {
				alone.push_back(ladders.front());
			}
		}

		// What each kind falls back on: the perk that unlocks most of the mods
		// it can take. Ties go to the perk that unlocks more of the load order,
		// never to the player's ranks, so the result is the same for everyone.
		for (std::size_t kind = 0; kind < KINDS; kind++) {
			std::uint32_t most = 0;
			for (const auto& said : byKind[kind]) {
				if (said.count > most ||
					(said.count == most && Weight(said.perk) > Weight(g_kinds[kind]))) {
					g_kinds[kind] = said.perk;
					most = said.count;
				}
			}
		}

		// The perks that can price a repair, for the description hook. A perk
		// that can never price one gets no line, which keeps Demolition Expert
		// unchanged.
		std::vector<const RE::BGSPerk*> saying;
		const auto remember = [&saying](const RE::BGSPerk* a_perk) {
			if (a_perk && std::find(saying.begin(), saying.end(), a_perk) == saying.end()) {
				saying.push_back(a_perk);
			}
		};
		for (const auto* perk : alone) {
			remember(perk);
		}
		for (const auto* perk : g_kinds) {
			remember(perk);
		}
		TellRanks(saying);

		REX::INFO("{:d} of the load order's weapon mod recipes name a crafting perk, across {:d} weapons that wear out, and {:d} perks now say so.",
			gated, weapons, saying.size());

		// Perk recipes that fit no weapon means the slots matched nothing, and
		// every repair would quietly cost full price.
		if (weapons > 0 && gated == 0) {
			REX::WARN("No weapon mod recipe in the load order reaches a weapon, so no repair will be discounted.");
		}

		for (const auto* first : saying) {
			const auto  ranks = Ranks(first);
			std::string steps;
			for (std::uint32_t at = 1; at <= ranks; at++) {
				steps += std::format("{:s}{:d}%", steps.empty() ? "" : " ", Discount(at, ranks));
			}
			REX::INFO("Crafting perk {:s} has {:d} ranks and gates {:d} recipes, taking off {:s}",
				RE::TESFullName::GetFullName(*first), ranks, Weight(first), steps);
		}
		for (std::size_t kind = 0; kind < KINDS; kind++) {
			if (g_kinds[kind]) {
				REX::INFO("A weapon of kind {:d} whose own parts name no perk falls back on {:s}",
					kind, RE::TESFullName::GetFullName(*g_kinds[kind]));
			}
		}
	}

	Standing Of(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra,
		std::span<const Materials::Line> a_bill)
	{
		Standing out;

		// IsWeapon reads the form type byte, so it costs nothing and never
		// comes back empty like a runtime cast can.
		if (!a_object.IsWeapon()) {
			return out;
		}
		const auto& weapon = static_cast<const RE::TESObjectWEAP&>(a_object);

		// Each line counts its component units for the perk its recipe needs. A
		// line whose recipe needs no perk counts for nobody.
		std::vector<Tally> tally;
		for (const auto& line : a_bill) {
			const auto part = line.recipe ? OfRecipe(*line.recipe) : Standing{};
			if (part.perk) {
				Note(tally, part.perk, line.count);
			}
		}

		// The most units wins. A tie goes to whichever costs the player less,
		// then to the perk that unlocks more of the load order.
		std::uint32_t most = 0;
		std::uint32_t heft = 0;
		auto          best = UNSKILLED_MULTIPLE;
		for (const auto& said : tally) {
			Standing standing;
			standing.perk = said.perk;
			standing.ranks = Ranks(said.perk);
			standing.rank = RankHeld(said.perk);

			const auto multiple = Multiple(standing);
			const auto weight = Weight(said.perk);
			const bool better = !out.perk || said.count > most ||
			                    (said.count == most && multiple < best) ||
			                    (said.count == most && multiple == best && weight > heft);
			if (better) {
				out = standing;
				most = said.count;
				best = multiple;
				heft = weight;
			}
		}

		// No part of the gun names a perk, so it is treated as any weapon of
		// its kind.
		if (!out.perk) {
			out.perk = g_kinds[KindOf(weapon, a_extra)];
			out.fromKind = out.perk != nullptr;
			out.ranks = Ranks(out.perk);
			out.rank = RankHeld(out.perk);
		}
		return out;
	}
}
