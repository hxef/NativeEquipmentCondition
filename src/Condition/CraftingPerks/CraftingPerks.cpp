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

		// The same for armor, which is one kind: the perk behind most armor
		// mods, Armorer in vanilla.
		const RE::BGSPerk* g_armor{ nullptr };

		// How many mod recipes each perk unlocks across the load order, weapon
		// mods and armor mods counted apart. The total breaks the tie for a
		// weapon that names 2 perks equally, like the Fat Man, half Gun Nut and
		// half Science: Gun Nut unlocks 370 and Science 284, 44 of them armor
		// mods, so it goes under Gun Nut, where vanilla puts it. The 2 counts
		// say what a perk's page should name.
		struct Gates
		{
			std::uint32_t weapons{ 0 };
			std::uint32_t armor{ 0 };
		};

		std::unordered_map<const RE::BGSPerk*, Gates> g_gates;

		// Every recipe that names a perk, and the perks it names. The game
		// states it on the recipe, so nothing here is guessed.
		std::unordered_map<const RE::BGSConstructibleObject*, std::vector<const RE::BGSPerk*>> g_byRecipe;

		// What one perk unlocks, looked up without adding an entry.
		[[nodiscard]] Gates GatesOf(const RE::BGSPerk* a_perk)
		{
			const auto found = a_perk ? g_gates.find(a_perk) : g_gates.end();
			return found != g_gates.end() ? found->second : Gates{};
		}

		// How many recipes one perk unlocks in all.
		[[nodiscard]] std::uint32_t Weight(const RE::BGSPerk* a_perk)
		{
			const auto gates = GatesOf(a_perk);
			return gates.weapons + gates.armor;
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

	bool PricesWeapons(const RE::BGSPerk* a_first)
	{
		return GatesOf(a_first).weapons > 0;
	}

	bool PricesArmor(const RE::BGSPerk* a_first)
	{
		return GatesOf(a_first).armor > 0;
	}

	void Unload()
	{
		g_kinds.fill(nullptr);
		g_armor = nullptr;
		g_gates.clear();
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

		// Which slots the weapons and armor that take part offer, so a recipe
		// can be matched to the kinds its mod could go on.
		const auto slots = SlotsOffered();

		// Every recipe that needs a perk counts that perk once for every kind
		// of weapon the mod it builds could go on, or once for armor.
		std::array<std::vector<Tally>, KINDS> byKind;
		std::vector<Tally>                    byArmor;
		std::vector<const RE::BGSPerk*>       ladders;
		std::vector<const RE::BGSPerk*>       alone;
		std::uint32_t                         gated = 0;
		std::uint32_t                         armorGated = 0;

		for (const auto* recipe : g_dataHandler->GetFormArray<RE::BGSConstructibleObject>()) {
			const auto* built = recipe ? recipe->createdItem : nullptr;
			if (!built) {
				continue;
			}

			LaddersOf(*recipe, perks, ladders);
			if (ladders.empty()) {
				continue;
			}

			// Kept whatever it builds, since the bench prices whatever recipe
			// is in front of it. The count below is stricter.
			g_byRecipe.emplace(recipe, ladders);

			const auto reach = ReachOf(*built, slots);
			if (reach.kinds == 0 && !reach.armor) {
				continue;
			}

			gated += reach.kinds != 0 ? 1 : 0;
			armorGated += reach.armor ? 1 : 0;
			for (const auto* ladder : ladders) {
				auto& gates = g_gates[ladder];
				gates.weapons += reach.kinds != 0 ? 1 : 0;
				gates.armor += reach.armor ? 1 : 0;
				for (std::size_t kind = 0; kind < KINDS; kind++) {
					if (reach.kinds & (1U << kind)) {
						Note(byKind[kind], ladder, 1);
					}
				}
				if (reach.armor) {
					Note(byArmor, ladder, 1);
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
		const auto winner = [](const std::vector<Tally>& a_said) {
			const RE::BGSPerk* best = nullptr;
			std::uint32_t      most = 0;
			for (const auto& said : a_said) {
				if (said.count > most ||
					(said.count == most && Weight(said.perk) > Weight(best))) {
					best = said.perk;
					most = said.count;
				}
			}
			return best;
		};
		for (std::size_t kind = 0; kind < KINDS; kind++) {
			g_kinds[kind] = winner(byKind[kind]);
		}
		g_armor = winner(byArmor);

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
		remember(g_armor);
		TellRanks(saying);

		REX::INFO("{:d} of the load order's weapon mod recipes name a crafting perk, across {:d} weapons that wear out, and {:d} armor mod recipes do, across {:d} pieces of armor that wear out. {:d} perks now say so.",
			gated, slots.weaponCount, armorGated, slots.armorCount, saying.size());

		// Perk recipes that fit nothing means the slots matched nothing, and
		// every repair would quietly cost full price.
		if (slots.weaponCount > 0 && gated == 0) {
			REX::WARN("No weapon mod recipe in the load order reaches a weapon, so no weapon repair will be discounted.");
		}
		if (slots.armorCount > 0 && armorGated == 0) {
			REX::WARN("No armor mod recipe in the load order reaches a piece of armor, so no armor repair will be discounted.");
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
		if (g_armor) {
			REX::INFO("A piece of armor whose own parts name no perk falls back on {:s}",
				RE::TESFullName::GetFullName(*g_armor));
		}
	}

	Standing Of(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra,
		std::span<const Materials::Line> a_bill)
	{
		Standing out;

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

		// No part of the item names a perk, so it is treated as any item of its
		// kind: a weapon by its type, a piece of armor as armor. IsWeapon reads
		// the form type byte, so it costs nothing and never comes back empty
		// like a runtime cast can.
		if (!out.perk) {
			out.perk = a_object.IsWeapon() ?
			               g_kinds[KindOf(static_cast<const RE::TESObjectWEAP&>(a_object), a_extra)] :
			               g_armor;
			out.fromKind = out.perk != nullptr;
			out.ranks = Ranks(out.perk);
			out.rank = RankHeld(out.perk);
		}
		return out;
	}
}
