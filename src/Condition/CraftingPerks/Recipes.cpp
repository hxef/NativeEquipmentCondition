#include "Condition/CraftingPerks/Recipes.h"

#include "Condition/CraftingPerks/Ladder.h"

#include <algorithm>

namespace CraftingPerks
{
	void LaddersOf(const RE::BGSConstructibleObject& a_recipe,
		const std::unordered_set<const RE::TESForm*>& a_perks,
		std::vector<const RE::BGSPerk*>&              a_out)
	{
		a_out.clear();
		for (const auto* item = a_recipe.conditions.head; item; item = item->next) {
			const auto& call = item->data.functionData;
			if (call.function.get() != RE::SCRIPT_OUTPUT::kScript_HasPerk) {
				continue;
			}

			const auto* named = static_cast<const RE::TESForm*>(call.param[0]);
			if (!named || !a_perks.contains(named)) {
				continue;
			}

			const auto* first = FirstRank(static_cast<const RE::BGSPerk*>(named));
			if (first && std::find(a_out.begin(), a_out.end(), first) == a_out.end()) {
				a_out.push_back(first);
			}
		}
	}

	const RE::BGSKeyword* PointOf(std::uint16_t a_index)
	{
		return RE::BGSKeyword::GetTypedKeywordByIndex(RE::KeywordType::kAttachPoint, a_index);
	}

	std::size_t KindOf(const RE::TESObjectWEAP& a_weapon, const RE::ExtraDataList* a_extra)
	{
		const auto* extra = a_extra ? a_extra->GetByType<RE::ExtraInstanceData>() : nullptr;
		const auto* built = extra ? static_cast<const RE::TESObjectWEAP::InstanceData*>(extra->data.get())
		                          : nullptr;
		const auto  kind = built ? built->type.underlying() : a_weapon.weaponData.type.underlying();
		return static_cast<std::size_t>(kind) < KINDS ? static_cast<std::size_t>(kind) : 0;
	}
}
