#include "Condition/Materials/Yield.h"

#include "Condition/Materials/Index.h"
#include "Condition/Repair.h"

#include <algorithm>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace Materials
{
	namespace
	{
		// The share of a count scrapping gives, fBuildableScrapCountMultBase
		// in vanilla, in tenths. A spare's count goes in divided by it, so
		// vanilla's count gives the whole count back.
		constexpr std::uint64_t HALVED_TENTHS = 5;

		// The forms a copy's recipes name and the count of each, added up
		// within a form and never across 2, as the bench tallies them.
		using Tally = std::vector<std::pair<RE::TESForm*, std::uint32_t>>;

		void Add(Tally& a_tally, const RE::BGSConstructibleObject& a_recipe)
		{
			if (!a_recipe.requiredItems) {
				return;
			}
			for (const auto& required : *a_recipe.requiredItems) {
				const auto count = required.second.i;
				if (!required.first || count <= 0) {
					continue;
				}
				const auto already = std::ranges::find(a_tally, required.first, &Tally::value_type::first);
				if (already != a_tally.end()) {
					already->second += static_cast<std::uint32_t>(count);
				} else {
					a_tally.emplace_back(required.first, static_cast<std::uint32_t>(count));
				}
			}
		}

		// The bench's walk, see Yield in Yield.h. ForEachRecipe prices an
		// item and borrows a recipe and skips the legendary one, which this
		// walk must not.
		Tally TallyOf(const Index& a_index, const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra)
		{
			Tally tally;
			const auto* mods = a_extra ? a_extra->GetByType<RE::BGSObjectInstanceExtra>() : nullptr;
			if (mods && mods->values) {
				for (const auto& entry : mods->GetIndexData()) {
					const auto* mod = entry.disabled ? nullptr : RE::TESForm::GetFormByID<RE::BGSMod::Attachment::Mod>(entry.objectID);
					const auto  found = mod ? a_index.mods.find(mod) : a_index.mods.end();
					if (found != a_index.mods.end()) {
						Add(tally, *found->second);
					}
				}
			}
			if (const auto* own = OwnRecipe(a_object)) {
				Add(tally, *own);
			}
			return tally;
		}
	}

	const RE::BGSComponent* LeastPart(std::span<const Tallied> a_tally)
	{
		const auto index = Current();
		if (!index || std::ranges::any_of(a_tally, [](const Tallied& a_form) { return a_form.given > 0; })) {
			return nullptr;
		}

		// Each component's count, its scrap item's added in.
		std::vector<std::pair<const RE::BGSComponent*, std::uint64_t>> held;
		for (const auto& [form, count, given] : a_tally) {
			const auto* component = PricedBy(*index, form);
			if (!component || !component->scrapItem || count == 0) {
				continue;
			}
			const auto already = std::ranges::find(held, component, &decltype(held)::value_type::first);
			if (already != held.end()) {
				already->second += count;
			} else {
				held.emplace_back(component, count);
			}
		}

		const RE::BGSComponent* most = nullptr;
		std::uint64_t           mostCount = 0;
		for (const auto& [component, count] : held) {
			if (!most || count > mostCount || (count == mostCount && component->GetFormID() < most->GetFormID())) {
				most = component;
				mostCount = count;
			}
		}
		return most;
	}

	std::uint32_t ScrapIn(std::uint32_t a_count, std::uint32_t a_percent)
	{
		// Whole numbers, never a float: 50 at 53% is 26.5 and rounds up to
		// 27, but 50 times 0.53F is 26.499998 and would round down to 26.
		const auto percent = std::min(a_percent, Repair::FULL);
		return static_cast<std::uint32_t>((std::uint64_t{ a_count } * percent + 50) / 100);
	}

	std::vector<Part> Yield(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra,
		std::uint32_t a_percent, std::uint32_t a_tenths)
	{
		std::vector<Part> out;
		const auto        index = Current();
		if (!index) {
			return out;
		}

		std::vector<Tallied> tallied;
		for (const auto& [form, count] : TallyOf(*index, a_object, a_extra)) {
			// A line for a finished item says nothing about what the copy is
			// made of, as in the bill.
			const auto* component = PricedBy(*index, form);
			if (!component) {
				continue;
			}
			// Whole numbers again: 5 Steel at 0.7F over 0.5 comes to 6.9999999
			// and would round down to 6, where 7 tenths give 7.
			const auto in = std::uint64_t{ ScrapIn(count, a_percent) } * a_tenths / HALVED_TENTHS;

			// The bench's flags: the halving on, and the component's share.
			const auto given = in > 0 ? RE::ScrapRewardCount(form, static_cast<std::uint32_t>(in), true, false) : 0;
			tallied.push_back({ form, count, given });
			if (given == 0) {
				continue;
			}
			const auto already = std::ranges::find(out, component, &Part::component);
			if (already != out.end()) {
				already->count += given;
			} else {
				out.push_back({ component, given });
			}
		}

		// The bench gives a new copy what the game gives, so only a worn one
		// gets the 1 component.
		const auto* least = a_percent < Repair::FULL ? LeastPart(tallied) : nullptr;
		if (least) {
			out.push_back({ least, 1 });
		}
		return out;
	}
}
