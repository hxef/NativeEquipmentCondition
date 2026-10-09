#include "Gameplay/SpawnCondition/Owners.h"

#include <initializer_list>

namespace SpawnCondition
{
	bool Players(const RE::BGSInventoryList* a_list)
	{
		return a_list && a_list->owner == RE::ObjectRefHandle{ RE::PlayerCharacter::GetPlayerHandle() };
	}

	const RE::TESActorBase* Essential(const RE::BGSInventoryList* a_list)
	{
		if (!a_list) {
			return nullptr;
		}
		const auto owner = a_list->owner.get();
		if (!owner) {
			return nullptr;
		}

		const RE::TESActorBase* base = nullptr;
		if (owner->extraList) {
			if (const auto* leveled = owner->extraList->GetByType<RE::ExtraLeveledCreature>()) {
				base = leveled->originalBase;
			}
		}
		if (!base) {
			const auto* object = owner->GetObjectReference();
			if (object && object->Is(RE::ENUM_FORM_ID::kNPC_)) {
				base = static_cast<const RE::TESNPC*>(object);
			}
		}
		return base && base->IsEssential() ? base : nullptr;
	}

	const RE::BGSOutfit* PlayersOutfit(const RE::ExtraDataList& a_extra)
	{
		const auto* player = RE::PlayerCharacter::GetSingleton();
		const auto* npc = player ? player->GetNPC() : nullptr;
		const auto* stamp = a_extra.GetByType<RE::ExtraOutfitItem>();
		if (!npc || !stamp) {
			return nullptr;
		}
		for (const auto* outfit : { npc->defOutfit, npc->sleepOutfit }) {
			if (outfit && outfit->formID == stamp->outfit) {
				return outfit;
			}
		}
		return nullptr;
	}

	bool Showpiece(const RE::BGSInventoryList* a_list, RE::ExtraDataList& a_extra)
	{
		if (!a_list) {
			return false;
		}
		const auto owner = a_list->owner.get();
		return owner && !owner->IsActor() && a_extra.GetLegendaryMod() != nullptr;
	}
}
