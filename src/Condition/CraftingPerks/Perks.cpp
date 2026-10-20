#include "Condition/CraftingPerks/Perks.h"

#include "Condition/CraftingPerks/Description.h"
#include "Condition/CraftingPerks/Recipes.h"

#include <algorithm>

namespace CraftingPerks
{
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

	Gates GatesOf(const Perks& a_perks, const RE::BGSPerk* a_perk)
	{
		const auto found = a_perk ? a_perks.gates.find(a_perk) : a_perks.gates.end();
		return found != a_perks.gates.end() ? found->second : Gates{};
	}

	std::uint32_t Weight(const Perks& a_perks, const RE::BGSPerk* a_perk)
	{
		const auto gates = GatesOf(a_perks, a_perk);
		return gates.weapons + gates.armor;
	}

	std::shared_ptr<Perks> Measure(Counted& a_counted)
	{
		auto perks = std::make_shared<Perks>();
		for (const auto* perk : g_dataHandler->GetFormArray<RE::BGSPerk>()) {
			if (perk) {
				perks->all.insert(perk);
			}
		}

		// Which slots the weapons and armor that take part offer, so a recipe
		// can be matched to the kinds its mod could go on.
		const auto slots = SlotsOffered();
		a_counted.weaponCount = slots.weaponCount;
		a_counted.armorCount = slots.armorCount;

		// Every recipe that needs a perk counts that perk once for every kind
		// of weapon the mod it builds could go on, or once for armor.
		std::array<std::vector<Tally>, KINDS> byKind;
		std::vector<Tally>                    byArmor;
		std::vector<const RE::BGSPerk*>       ladders;
		std::vector<const RE::BGSPerk*>       alone;

		for (const auto* recipe : g_dataHandler->GetFormArray<RE::BGSConstructibleObject>()) {
			const auto* built = recipe ? recipe->createdItem : nullptr;
			if (!built) {
				continue;
			}

			LaddersOf(*recipe, perks->all, ladders);
			if (ladders.empty()) {
				continue;
			}

			const auto reach = ReachOf(*built, slots);
			if (reach.kinds == 0 && !reach.armor) {
				continue;
			}

			a_counted.gated += reach.kinds != 0 ? 1 : 0;
			a_counted.armorGated += reach.armor ? 1 : 0;
			for (const auto* ladder : ladders) {
				auto& gates = perks->gates[ladder];
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
		const auto winner = [&perks](const std::vector<Tally>& a_said) {
			const RE::BGSPerk* best = nullptr;
			std::uint32_t      most = 0;
			for (const auto& said : a_said) {
				if (said.count > most ||
					(said.count == most && Weight(*perks, said.perk) > Weight(*perks, best))) {
					best = said.perk;
					most = said.count;
				}
			}
			return best;
		};
		for (std::size_t kind = 0; kind < KINDS; kind++) {
			perks->kinds[kind] = winner(byKind[kind]);
		}
		perks->armor = winner(byArmor);

		// The perks that can price a repair, for the description hook. A perk
		// that can never price one gets no line, which keeps Demolition Expert
		// unchanged.
		auto&      saying = a_counted.saying;
		const auto remember = [&saying](const RE::BGSPerk* a_perk) {
			if (a_perk && std::find(saying.begin(), saying.end(), a_perk) == saying.end()) {
				saying.push_back(a_perk);
			}
		};
		for (const auto* perk : alone) {
			remember(perk);
		}
		for (const auto* perk : perks->kinds) {
			remember(perk);
		}
		remember(perks->armor);
		TellRanks(*perks, saying);
		return perks;
	}
}
