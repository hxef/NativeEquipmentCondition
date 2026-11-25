#include "Condition/WearList/WearList.h"

#include "Condition/WearList/Table.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>

namespace WearList
{
	namespace
	{
		// Whether a_set was read before a_first, by file and then line, or
		// a_first is empty.
		bool Earlier(const std::optional<Set>& a_set, const std::optional<Set>& a_first)
		{
			return a_set && (!a_first || std::tie(a_set->file, a_set->line) < std::tie(a_first->file, a_first->line));
		}

		// The lines for 1 copy of a_item, a weapon or a piece of armor, whose
		// own lines are in a_own. a_floor is the number a line keeps at 0
		// condition for its kind, damage or protection, and goes to
		// a_kept. Nothing is kept per copy: the game frees a copy's instance
		// data and builds it again at every mod change, and may give the new
		// one the old address, so no key would outlive its mods. The table is
		// the cache.
		template <class Item>
		Pick PickOf(const Table& a_table, const std::unordered_map<RE::TESFormID, Entry>& a_own, const Item& a_item,
			const typename Item::InstanceData* a_instance, const RE::ExtraDataList* a_extra, std::optional<Set> Entry::* a_floor,
			std::optional<Set> Pick::* a_kept)
		{
			// The item's own line, else for each number the keyword line read
			// first of the keywords the copy has. Its keywords come from the
			// instance, so a mod that adds one counts.
			Pick               pick;
			auto&              kept = pick.*a_kept;
			std::optional<Set> wear;
			std::optional<Set> keywordFloor;
			if (const auto own = a_own.find(a_item.formID); own != a_own.end()) {
				wear = own->second.wear;
				kept = own->second.*a_floor;
			}
			const auto ownWear = wear.has_value();
			for (const auto& [keyword, entry] : a_table.keywords) {
				const auto takeWear = !ownWear && Earlier(entry.wear, wear);
				const auto takeFloor = !kept && Earlier(entry.*a_floor, keywordFloor);
				if ((takeWear || takeFloor) && a_item.HasKeyword(keyword, a_instance)) {
					wear = takeWear ? entry.wear : wear;
					keywordFloor = takeFloor ? entry.*a_floor : keywordFloor;
				}
			}
			double product = 1.0;
			if (wear) {
				product = wear->value;
				pick.wearFrom[pick.wearCount++] = *wear;
			}

			// Then each enabled mod's line, as Materials walks them. A legendary
			// mod counts too, unlike there, since a player may list one. An item
			// with no mods may come back from a save with no mod list, which
			// GetIndexData reads without checking. The product is a double, so no
			// count of mods reaches infinity, and a mod at 0 makes it 0.
			std::optional<Set> modFloor;
			const auto*        mods = a_extra ? a_extra->GetByType<RE::BGSObjectInstanceExtra>() : nullptr;
			if (mods && mods->values && !a_table.mods.empty()) {
				for (const auto& index : mods->GetIndexData()) {
					const auto found = index.disabled ? a_table.mods.end() : a_table.mods.find(index.objectID);
					if (found == a_table.mods.end()) {
						continue;
					}
					const auto& modWear = found->second.wear;
					if (modWear) {
						product *= modWear->value;
						if (pick.wearCount < Pick::MOST_FROM) {
							pick.wearFrom[pick.wearCount] = *modWear;
						}
						pick.wearCount++;
					}
					if (Earlier(found->second.*a_floor, modFloor)) {
						modFloor = found->second.*a_floor;
					}
				}
			}

			kept = kept ? kept : modFloor ? modFloor : keywordFloor;
			pick.capped = product > MOST_WEAR;
			pick.wear = static_cast<float>(std::min(product, static_cast<double>(MOST_WEAR)));
			return pick;
		}
	}

	Pick Of(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_instance, const RE::ExtraDataList* a_extra)
	{
		const auto table = Any() ? Current() : nullptr;
		return table ? PickOf(*table, table->weapons, a_weapon, a_instance, a_extra, &Entry::damage, &Pick::damage) : Pick{};
	}

	Pick Of(const RE::TESObjectARMO& a_armor, const RE::TESObjectARMO::InstanceData* a_instance, const RE::ExtraDataList* a_extra)
	{
		const auto table = Any() ? Current() : nullptr;
		return table ? PickOf(*table, table->armors, a_armor, a_instance, a_extra, &Entry::protection, &Pick::protection) : Pick{};
	}

	std::string Pick::WearTail() const
	{
		const auto table = wearCount > 0 ? Current() : nullptr;
		if (!table) {
			return {};
		}
		// 1 line names no factor of its own, since it is the whole product.
		// Past MOST_FROM the lines are only counted.
		const auto  named = std::min(wearCount, MOST_FROM);
		const auto  more = wearCount - named;
		std::string text = std::format(", wear x{:.2f} by ", wear);
		for (std::size_t i = 0; i < named; i++) {
			if (i > 0) {
				text += i + 1 == named && more == 0 ? " and " : ", ";
			}
			text += Where(*table, wearFrom[i], i > 0 ? &wearFrom[i - 1] : nullptr);
			if (wearCount > 1) {
				text += std::format(" x{:.2f}", wearFrom[i].value);
			}
		}
		if (more > 0) {
			text += std::format(" and {:d} more {:s}", more, more == 1 ? "line" : "lines");
		}
		if (capped) {
			text += " up to 10";
		}
		return text;
	}

	std::string DamageTail(const std::optional<Set>& a_damage)
	{
		const auto table = a_damage ? Current() : nullptr;
		return table ? std::format("  keeps {:.2f} at 0 by {:s}", a_damage->value, Where(*table, *a_damage, nullptr)) : std::string{};
	}
}
