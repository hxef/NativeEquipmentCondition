#include "Condition/Materials/Materials.h"

#include "Condition/Materials/Index.h"

#include <algorithm>

namespace Materials
{
	std::vector<Line> BillOfLines(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra)
	{
		std::vector<Line> bill;
		ForEachRecipe(a_object, a_extra, [&bill](const RE::BGSConstructibleObject& a_recipe) {
			if (!a_recipe.requiredItems) {
				return;
			}

			for (const auto& required : *a_recipe.requiredItems) {
				// A line for a finished item or for nothing at all says nothing
				// about what the item is made of.
				const auto* component = PricedBy(required.first);
				const auto  count = required.second.i;
				if (!component || count == 0) {
					continue;
				}

				// One recipe naming the same component twice is merged into one
				// line, since nothing later could tell the 2 apart.
				const auto already = std::find_if(bill.begin(), bill.end(),
					[&a_recipe, component](const Line& a_line) {
						return a_line.recipe == &a_recipe && a_line.component == component;
					});
				if (already != bill.end()) {
					already->count += count;
				} else {
					bill.push_back({ &a_recipe, component, static_cast<std::uint32_t>(count) });
				}
			}
		});
		return bill;
	}

	std::vector<Part> BillOfParts(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra)
	{
		return BillOfParts(BillOfLines(a_object, a_extra));
	}

	std::vector<Part> BillOfParts(std::span<const Line> a_bill)
	{
		// A weapon's scrap recipe and its mods often name the same steel, and
		// the bill lists it once.
		std::vector<Part> bill;
		for (const auto& line : a_bill) {
			const auto already = std::find_if(bill.begin(), bill.end(),
				[&line](const Part& a_part) { return a_part.component == line.component; });
			if (already != bill.end()) {
				already->count += line.count;
			} else {
				bill.push_back({ line.component, line.count });
			}
		}
		return bill;
	}

	std::vector<const RE::BGSComponent*> Order(std::span<const Part> a_bill, std::uint32_t a_units)
	{
		// Where 1 unit of a component falls in the order. A component with a
		// count of n places its units at 0.5/n, 1.5/n and so on, so a component
		// with more units comes up sooner and more often.
		struct Seat
		{
			float                   at;
			std::uint32_t           count;
			std::uint32_t           id;
			const RE::BGSComponent* component;

			[[nodiscard]] bool operator<(const Seat& a_other) const
			{
				if (at != a_other.at) {
					return at < a_other.at;
				}
				if (count != a_other.count) {
					return count > a_other.count;
				}
				return id < a_other.id;
			}
		};

		// Every component gets as many places as a_units asks for, since a
		// weapon built from a single component takes them all.
		std::vector<Seat> seats;
		seats.reserve(a_bill.size() * a_units);
		for (const auto& part : a_bill) {
			if (!part.component || part.count == 0) {
				continue;
			}

			const auto count = static_cast<float>(part.count);
			for (std::uint32_t seat = 1; seat <= a_units; seat++) {
				seats.push_back({ (static_cast<float>(seat) - 0.5F) / count, part.count,
					part.component->formID, part.component });
			}
		}

		std::sort(seats.begin(), seats.end());
		seats.resize(std::min<std::size_t>(seats.size(), a_units));

		std::vector<const RE::BGSComponent*> out;
		out.reserve(seats.size());
		for (const auto& seat : seats) {
			out.push_back(seat.component);
		}
		return out;
	}
}
