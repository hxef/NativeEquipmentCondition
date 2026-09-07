#include "Events.h"
#include "Common.h"
#include "Degradation.h"

namespace Events
{

	RE::BSEventNotifyControl All::OnPlayerMeleeHitEvent::ProcessEvent(const RE::TESHitEvent& a_event, RE::BSTEventSource<RE::TESHitEvent>*)
	{
		// Check if player hit someone with a melee weapon. Degrade the weapon's condition if so.
		if (a_event.usesHitData && a_event.hitData.aggressor.get().get() == g_player) {
			Degradation::ForEachStackWithLock(
				g_player->inventoryList,

				// Iterate over melee weapons.
				[](RE::BGSInventoryItem& item) {
					if (item.object->IsWeapon()) {
						switch (static_cast<RE::TESObjectWEAP*>(item.object)->weaponData.type.get()) {
						case RE::WEAPON_TYPE::kHandToHand:
						case RE::WEAPON_TYPE::kOneHandSword:
						case RE::WEAPON_TYPE::kOneHandDagger:
						case RE::WEAPON_TYPE::kOneHandAxe:
						case RE::WEAPON_TYPE::kOneHandMace:
						case RE::WEAPON_TYPE::kTwoHandSword:
						case RE::WEAPON_TYPE::kTwoHandAxe:
							return true;
						default:
							return false;
					}
					return false;
				} },

				// Decrease condition of the equipped melee weapon.
				[](RE::BGSInventoryItem& item, RE::BGSInventoryItem::Stack& stack) {
					if (stack.IsEquipped()) {
						Degradation::WeaponCondition(stack).Decrease(*item.object);
						return false;  // Stop iteration.
					}
					return true;  // Continue searching...
				}

			);
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl All::EquipWatcher::ProcessEvent(const RE::TESEquipEvent& a_event, RE::BSTEventSource<RE::TESEquipEvent>*)
	{
		// NOTE: this handler is still a stub. It looks the equipped form up and then
		// discards it. Carried over from the 2023 work as-is.
		if (a_event.actor.get() == g_player && a_event.equipped) {
			const auto& item = RE::TESForm::GetFormByID<RE::TESObjectWEAP>(a_event.baseObject);
			(void)item;
		}
		return RE::BSEventNotifyControl::kContinue;
	}
}
