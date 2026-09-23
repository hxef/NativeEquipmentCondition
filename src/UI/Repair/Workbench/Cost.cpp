#include "UI/Repair/Workbench/Cost.h"

#include "Condition/Repair.h"
#include "Core/Settings.h"

#include <algorithm>
#include <format>
#include <utility>

namespace Workbench
{
	namespace
	{
		using Repair::Debt;
		using Repair::Whole;

		// The most fBenchCostMult counts for. The order lays a bill out one unit
		// at a time, so a multiple in the millions runs the game out of memory.
		constexpr float MAX_COST_MULT = 10.0F;

		// The whole number of units a weapon at a condition still owes, rounded
		// once on the whole bill.
		[[nodiscard]] std::uint32_t Owed(const Priced& a_priced, std::uint32_t a_percent)
		{
			const auto units = static_cast<float>(a_priced.units);
			return std::min<std::uint32_t>(Whole(units * Debt(a_percent, a_priced.multiple)),
				static_cast<std::uint32_t>(a_priced.order.size()));
		}
	}

	float Scaled(float a_multiple)
	{
		const auto mult = Settings::fBenchCostMult.GetValue();
		return a_multiple * (mult > 0.0F ? std::min(mult, MAX_COST_MULT) : 0.0F);
	}

	Priced PriceOf(const Selection& a_selection)
	{
		Priced out;
		if (!a_selection.object) {
			return out;
		}

		// Its own scrap recipe plus the recipe behind every mod, so the bill
		// reads as a smaller copy of the gun.
		const auto bill = Materials::BillOfLines(*a_selection.object, a_selection.extra);
		out.built = Materials::BillOfParts(bill);
		out.standing = CraftingPerks::Of(*a_selection.object, a_selection.extra, bill);
		out.multiple = Scaled(CraftingPerks::Multiple(out.standing));

		for (const auto& part : out.built) {
			out.units += part.count;
		}
		out.order = Materials::Order(out.built,
			Whole(static_cast<float>(out.units) * Debt(0, out.multiple)) + 1);
		return out;
	}

	std::vector<Materials::Part> CostOf(const Priced& a_priced, std::uint32_t a_from, std::uint32_t a_to)
	{
		if (a_priced.order.empty() || a_to <= a_from) {
			return {};
		}

		const auto left = Owed(a_priced, a_to);
		auto       owed = Owed(a_priced, a_from);
		if (owed <= left) {
			owed = std::min<std::uint32_t>(left + 1,
				static_cast<std::uint32_t>(a_priced.order.size()));
		}

		std::vector<Materials::Part> out;
		for (auto at = left; at < owed; at++) {
			const auto* component = a_priced.order[at];
			const auto  already = std::find_if(out.begin(), out.end(),
				 [component](const Materials::Part& a_part) { return a_part.component == component; });
			if (already != out.end()) {
				already->count++;
			} else {
				out.push_back({ component, 1 });
			}
		}
		return out;
	}

	std::vector<std::uint32_t> Above(const Selection& a_selection)
	{
		return Repair::Above(a_selection.percent);
	}

	std::vector<std::uint32_t> Offered(const Selection& a_selection)
	{
		const auto priced = PriceOf(a_selection);

		std::vector<Repair::Step<std::vector<Materials::Part>>> steps;
		for (const auto level : Above(a_selection)) {
			steps.push_back({ level, CostOf(priced, a_selection.percent, level) });
		}

		std::vector<std::uint32_t> out;
		for (const auto& step : Repair::Distinct(std::move(steps))) {
			out.push_back(step.level);
		}
		return out;
	}

	std::string Spell(const std::vector<Materials::Part>& a_parts)
	{
		std::string out;
		for (const auto& part : a_parts) {
			if (!part.component) {
				continue;
			}
			out += std::format("{:s}{:d} {:s}", out.empty() ? "" : ", ", part.count,
				RE::TESFullName::GetFullName(*part.component));
		}
		return out.empty() ? "nothing" : out;
	}

	std::string Vote(const Selection& a_selection)
	{
		if (!a_selection.object) {
			return "nothing";
		}

		struct Hand
		{
			const RE::BGSPerk* perk;
			std::uint32_t      units;
		};

		std::vector<Hand> hands;
		for (const auto& line : Materials::BillOfLines(*a_selection.object, a_selection.extra)) {
			const auto part = line.recipe ? CraftingPerks::OfRecipe(*line.recipe)
			                              : CraftingPerks::Standing{};
			const auto found = std::find_if(hands.begin(), hands.end(),
				[&part](const Hand& a_hand) { return a_hand.perk == part.perk; });
			if (found != hands.end()) {
				found->units += line.count;
			} else {
				hands.push_back({ part.perk, line.count });
			}
		}

		std::sort(hands.begin(), hands.end(),
			[](const Hand& a_left, const Hand& a_right) { return a_left.units > a_right.units; });

		std::string out;
		for (const auto& hand : hands) {
			out += std::format("{:s}{:s} {:d}", out.empty() ? "" : ", ",
				hand.perk ? RE::TESFullName::GetFullName(*hand.perk) : "nobody", hand.units);
		}
		return out.empty() ? "nothing" : out;
	}
}
