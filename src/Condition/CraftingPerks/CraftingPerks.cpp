#include "Condition/CraftingPerks/CraftingPerks.h"

#include "Condition/Condition.h"
#include "Condition/CraftingPerks/Description.h"
#include "Condition/CraftingPerks/Ladder.h"
#include "Condition/CraftingPerks/Perks.h"
#include "Condition/CraftingPerks/Recipes.h"
#include "Core/Settings.h"
#include "Core/Snapshot.h"

#include <algorithm>
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

		// The most iPerkComponentDiscount counts for. Never 100: a multiple of
		// 0 would make a paid repair that takes no components, a path the
		// bench's press never meets.
		constexpr std::int32_t MOST_DISCOUNT = 90;

		// iPerkComponentDiscount kept between 0 and MOST_DISCOUNT: what the
		// top rank takes off, in whole percent.
		std::uint32_t TopDiscount()
		{
			return static_cast<std::uint32_t>(std::clamp(Settings::iPerkComponentDiscount.GetValue(), 0, MOST_DISCOUNT));
		}

		// Where the player stands on the perk a_recipe names, by the perks of
		// a_perks that can price a repair. Read off the recipe's conditions at
		// each call, so a recipe changed after the last measure counts at once.
		Standing OfRecipe(const Perks& a_perks, const RE::BGSConstructibleObject& a_recipe)
		{
			Standing                        out;
			std::vector<const RE::BGSPerk*> ladders;
			LaddersOf(a_recipe, a_perks.pricing, ladders);

			// A part can need 2 perks, a recon scope Gun Nut for the mount and
			// Science for the glass, and about 130 recipes do. The one the
			// player has taken furthest counts, judged by the share of its
			// ranks held and not by rank number. That is the cheaper one while
			// the discount is above 0, and the pick holds at 0, where every
			// rank costs the same. 2 equal shares cost the same, so there is no
			// tie break.
			auto best = 0.0F;
			for (const auto* ladder : ladders) {
				Standing standing;
				standing.perk = ladder;
				standing.ranks = Ranks(ladder);
				standing.rank = RankHeld(ladder);

				const auto share = standing.Share();
				if (!out.perk || share > best) {
					best = share;
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
			if (a_perks.scrapper.empty()) {
				REX::INFO("Fallout4.esm has no Scrapper perk, so no perk page tells what Scrapper adds to a spare.");
			} else {
				REX::DEBUG("Perk {:s} has {:d} ranks, which tell what it adds to a spare",
					RE::TESFullName::GetFullName(*a_perks.scrapper.front()), a_perks.scrapper.size());
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

	Standing Scrapper()
	{
		const auto perks = Current();
		if (!perks || perks->scrapper.empty()) {
			return {};
		}
		Standing out;
		out.perk = perks->scrapper.front();
		out.rank = RankHeld(out.perk);
		out.ranks = static_cast<std::uint32_t>(perks->scrapper.size());
		return out;
	}

	float SkilledMultiple()
	{
		return UNSKILLED_MULTIPLE * (1.0F - static_cast<float>(TopDiscount()) / 100.0F);
	}

	float Multiple(const Standing& a_standing)
	{
		return UNSKILLED_MULTIPLE - (UNSKILLED_MULTIPLE - SkilledMultiple()) * a_standing.Share();
	}

	std::uint32_t Discount(std::uint32_t a_rank, std::uint32_t a_ranks)
	{
		// In whole numbers, so an exact half such as 12.5 always reads 13.
		if (a_ranks == 0) {
			return 0;
		}
		const auto held = std::min(a_rank, a_ranks);
		return (TopDiscount() * held * 2 + a_ranks) / (2 * a_ranks);
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
		                   was->told != now->told || was->scrapper != now->scrapper;
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

		// The most units wins. A tie goes to the perk the player has taken
		// furthest by the share of its ranks, which costs less while the
		// discount is above 0, then to the perk that unlocks more of the load
		// order.
		std::uint32_t most = 0;
		std::uint32_t heft = 0;
		auto          best = 0.0F;
		for (const auto& said : tally) {
			Standing standing;
			standing.perk = said.perk;
			standing.ranks = Ranks(said.perk);
			standing.rank = RankHeld(said.perk);

			const auto share = standing.Share();
			const auto weight = Weight(*perks, said.perk);
			const bool better = !out.perk || said.count > most ||
			                    (said.count == most && share > best) ||
			                    (said.count == most && share == best && weight > heft);
			if (better) {
				out = standing;
				most = said.count;
				best = share;
				heft = weight;
			}
		}

		// The rank the item asks for is the highest any of its parts' recipes
		// names of the winner, a 2 perk recipe that voted for the other perk
		// included.
		for (const auto& line : a_bill) {
			if (out.perk && line.recipe) {
				out.asked = std::max(out.asked, RankAsked(*line.recipe, out.perk));
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
