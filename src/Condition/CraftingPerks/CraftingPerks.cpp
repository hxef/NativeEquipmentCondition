#include "Condition/CraftingPerks/CraftingPerks.h"

#include "Condition/Condition.h"
#include "Condition/CraftingPerks/Description.h"
#include "Condition/CraftingPerks/Ladder.h"
#include "Condition/CraftingPerks/Perks.h"
#include "Condition/CraftingPerks/Recipes.h"
#include "Core/Snapshot.h"

#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

namespace CraftingPerks
{
	namespace
	{
		// The gates, fallbacks and perk page lines the bench and the perk page
		// read, see Perks.h.
		Snapshot<Perks> g_perks;

		// Where the player stands on the perk a_recipe names, by the perks of
		// a_perks. Read off the recipe's conditions at each call, so a recipe
		// changed after the last measure counts at once.
		Standing OfRecipe(const Perks& a_perks, const RE::BGSConstructibleObject& a_recipe)
		{
			Standing                        out;
			std::vector<const RE::BGSPerk*> ladders;
			LaddersOf(a_recipe, a_perks.all, ladders);

			// A part can need 2 perks, a recon scope Gun Nut for the mount and
			// Science for the glass, and about 130 recipes do. The one the
			// player has taken furthest counts, judged by what it does to the
			// price and not by rank number. 2 that come out equal cost the
			// same, so there is no tie break.
			auto best = UNSKILLED_MULTIPLE;
			for (const auto* ladder : ladders) {
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

		// What Load says of a measure in NEC.log.
		void Say(const Perks& a_perks, const Counted& a_counted)
		{
			REX::INFO("{:d} of the load order's weapon mod recipes name a crafting perk, across {:d} weapons that wear out, and {:d} armor mod recipes do, across {:d} pieces of armor that wear out. {:d} perks now say so.",
				a_counted.gated, a_counted.weaponCount, a_counted.armorGated, a_counted.armorCount, a_counted.saying.size());

			// Perk recipes that fit nothing means the slots matched nothing, and
			// every repair would quietly cost full price.
			if (a_counted.weaponCount > 0 && a_counted.gated == 0) {
				REX::WARN("No weapon mod recipe in the load order reaches a weapon, so no weapon repair will be discounted.");
			}
			if (a_counted.armorCount > 0 && a_counted.armorGated == 0) {
				REX::WARN("No armor mod recipe in the load order reaches a piece of armor, so no armor repair will be discounted.");
			}

			for (const auto* first : a_counted.saying) {
				const auto  ranks = Ranks(first);
				std::string steps;
				for (std::uint32_t at = 1; at <= ranks; at++) {
					steps += std::format("{:s}{:d}%", steps.empty() ? "" : " ", Discount(at, ranks));
				}
				REX::INFO("Crafting perk {:s} has {:d} ranks and gates {:d} recipes, taking off {:s}",
					RE::TESFullName::GetFullName(*first), ranks, Weight(a_perks, first), steps);
			}
			for (std::size_t kind = 0; kind < KINDS; kind++) {
				if (a_perks.kinds[kind]) {
					REX::INFO("A weapon of kind {:d} whose own parts name no perk falls back on {:s}",
						kind, RE::TESFullName::GetFullName(*a_perks.kinds[kind]));
				}
			}
			if (a_perks.armor) {
				REX::INFO("A piece of armor whose own parts name no perk falls back on {:s}",
					RE::TESFullName::GetFullName(*a_perks.armor));
			}
		}
	}

	std::shared_ptr<const Perks> Current()
	{
		return g_perks.Get();
	}

	Standing OfRecipe(const RE::BGSConstructibleObject& a_recipe)
	{
		const auto perks = Current();
		return perks ? OfRecipe(*perks, a_recipe) : Standing{};
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
		g_perks.Publish(nullptr);
	}

	void Load()
	{
		Unload();
		if (!g_dataHandler) {
			return;
		}

		Counted counted;
		auto    perks = Measure(counted);
		Say(*perks, counted);
		g_perks.Publish(std::move(perks));
	}

	std::vector<std::string> MeasureAgain()
	{
		const auto was = Current();
		if (!was) {
			return {};
		}
		Counted    counted;
		auto       now = Measure(counted);
		const bool moved = was->gates != now->gates || was->kinds != now->kinds || was->armor != now->armor ||
		                   was->told != now->told;
		g_perks.Publish(std::move(now));
		return moved ? std::vector<std::string>{ "the crafting perk gates" } : std::vector<std::string>{};
	}

	Standing Of(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra,
		std::span<const Materials::Line> a_bill)
	{
		Standing   out;
		const auto perks = Current();
		if (!perks) {
			return out;
		}

		// Each line counts its component units for the perk its recipe needs. A
		// line whose recipe needs no perk counts for nobody.
		std::vector<Tally> tally;
		for (const auto& line : a_bill) {
			const auto part = line.recipe ? OfRecipe(*perks, *line.recipe) : Standing{};
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
			const auto weight = Weight(*perks, said.perk);
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
		// kind: a weapon by its type, a piece of armor as armor.
		if (!out.perk) {
			out.perk = a_object.IsWeapon() ?
			               perks->kinds[KindOf(static_cast<const RE::TESObjectWEAP&>(a_object), a_extra)] :
			               perks->armor;
			out.fromKind = out.perk != nullptr;
			out.ranks = Ranks(out.perk);
			out.rank = RankHeld(out.perk);
		}
		return out;
	}
}
