#include "Condition/WearList/WearList.h"

#include "Condition/WearList/Table.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <tuple>

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
	}

	// Nothing is kept per copy: the game frees a copy's instance data and
	// builds it again at every mod change, and may give the new one the old
	// address, so no key would outlive its mods. The table is the cache.
	Pick Of(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_instance, const RE::ExtraDataList* a_extra)
	{
		Pick pick;
		const auto table = Any() ? Current() : nullptr;
		if (!table) {
			return pick;
		}

		// The weapon's own line, else for each number the keyword line read
		// first of the keywords the copy has. Its keywords come from the
		// instance, so a mod that adds one counts.
		std::optional<Set> wear;
		std::optional<Set> keywordDamage;
		if (const auto own = table->weapons.find(a_weapon.formID); own != table->weapons.end()) {
			wear = own->second.wear;
			pick.damage = own->second.damage;
		}
		const auto ownWear = wear.has_value();
		for (const auto& [keyword, entry] : table->keywords) {
			const auto takeWear = !ownWear && Earlier(entry.wear, wear);
			const auto takeDamage = !pick.damage && Earlier(entry.damage, keywordDamage);
			if ((takeWear || takeDamage) && a_weapon.HasKeyword(keyword, a_instance)) {
				wear = takeWear ? entry.wear : wear;
				keywordDamage = takeDamage ? entry.damage : keywordDamage;
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
		std::optional<Set> modDamage;
		const auto*        mods = a_extra ? a_extra->GetByType<RE::BGSObjectInstanceExtra>() : nullptr;
		if (mods && mods->values && !table->mods.empty()) {
			for (const auto& index : mods->GetIndexData()) {
				const auto found = index.disabled ? table->mods.end() : table->mods.find(index.objectID);
				if (found == table->mods.end()) {
					continue;
				}
				const auto& [modWear, damage] = found->second;
				if (modWear) {
					product *= modWear->value;
					if (pick.wearCount < Pick::MOST_FROM) {
						pick.wearFrom[pick.wearCount] = *modWear;
					}
					pick.wearCount++;
				}
				if (Earlier(damage, modDamage)) {
					modDamage = damage;
				}
			}
		}

		pick.damage = pick.damage ? pick.damage : modDamage ? modDamage : keywordDamage;
		pick.capped = product > MOST_WEAR;
		pick.wear = static_cast<float>(std::min(product, static_cast<double>(MOST_WEAR)));
		return pick;
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
