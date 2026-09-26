#include "Condition/CraftingPerks/Recipes.h"

#include "Condition/Condition.h"
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
		const auto& built = Condition::InstanceDataOf<RE::TESObjectWEAP::InstanceData>(a_extra, a_weapon.weaponData);
		const auto  kind = built.type.underlying();
		return static_cast<std::size_t>(kind) < KINDS ? static_cast<std::size_t>(kind) : 0;
	}

	Slots SlotsOffered()
	{
		Slots slots;
		if (!g_dataHandler) {
			return slots;
		}

		for (const auto* weapon : g_dataHandler->GetFormArray<RE::TESObjectWEAP>()) {
			if (!weapon || !Condition::WearsOut(*weapon)) {
				continue;
			}
			slots.weaponCount++;

			const auto  bit = 1U << KindOf(*weapon, nullptr);
			const auto& parents = weapon->attachParents;
			for (std::uint32_t i = 0; parents.array && i < parents.size; i++) {
				if (const auto* point = PointOf(parents.array[i].keywordIndex)) {
					slots.weapons[point] |= bit;
				}
			}
		}
		for (const auto* armor : g_dataHandler->GetFormArray<RE::TESObjectARMO>()) {
			if (!armor || !Condition::WearsOut(*armor)) {
				continue;
			}
			slots.armorCount++;

			const auto& parents = armor->attachParents;
			for (std::uint32_t i = 0; parents.array && i < parents.size; i++) {
				if (const auto* point = PointOf(parents.array[i].keywordIndex)) {
					slots.armor.insert(point);
				}
			}
		}
		return slots;
	}

	namespace
	{
		// What one mod can go on: the kinds of weapon offering its slot for a
		// weapon mod, or armor for an armor mod whose slot some piece offers.
		Reach ReachOfMod(const RE::TESForm* a_built, const Slots& a_slots)
		{
			Reach reach;
			if (!a_built || !a_built->Is(RE::ENUM_FORM_ID::kOMOD)) {
				return reach;
			}

			const auto& mod = *static_cast<const RE::BGSMod::Attachment::Mod*>(a_built);
			const auto* point = PointOf(mod.attachPoint.keywordIndex);
			if (!point) {
				return reach;
			}
			if (mod.targetFormType.get() == RE::ENUM_FORM_ID::kWEAP) {
				const auto found = a_slots.weapons.find(point);
				reach.kinds = found != a_slots.weapons.end() ? found->second : 0;
			} else if (mod.targetFormType.get() == RE::ENUM_FORM_ID::kARMO) {
				reach.armor = a_slots.armor.contains(point);
			}
			return reach;
		}
	}

	Reach ReachOf(const RE::TESForm& a_built, const Slots& a_slots)
	{
		// The same 2 forms Materials::Load reads: the item itself, or a list of
		// items.
		if (!a_built.Is(RE::ENUM_FORM_ID::kFLST)) {
			return ReachOfMod(&a_built, a_slots);
		}

		Reach reach;
		for (const auto* listed : static_cast<const RE::BGSListForm&>(a_built).arrayOfForms) {
			const auto one = ReachOfMod(listed, a_slots);
			reach.kinds |= one.kinds;
			reach.armor = reach.armor || one.armor;
		}
		return reach;
	}
}
