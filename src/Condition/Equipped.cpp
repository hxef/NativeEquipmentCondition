#include "Condition/Equipped.h"

#include "Condition/Condition.h"

#include <algorithm>

namespace Equipped
{
	namespace
	{
		// Whether this copy of a weapon casts a_effect, as its own object
		// effect or one a mod adds. A copy with no mods reads the weapon's own
		// data.
		bool Casts(const RE::TESObjectWEAP& a_weapon, const RE::BGSInventoryItem::Stack& a_stack,
			const RE::EnchantmentItem& a_effect)
		{
			if (a_weapon.GetBaseEnchanting() == &a_effect) {
				return true;
			}

			const auto* extra = a_stack.extra ? a_stack.extra->GetByType<RE::ExtraInstanceData>() : nullptr;
			const auto* data = extra && extra->data ?
			                       static_cast<const RE::TESObjectWEAP::InstanceData*>(extra->data.get()) :
			                       &a_weapon.weaponData;
			const auto* effects = data->GetEnchantmentArray();
			return effects && std::ranges::find(*effects, &a_effect) != effects->end();
		}
	}

	float WeaponHealth(RE::Actor* a_actor, const RE::TESForm* a_weapon, const RE::EnchantmentItem* a_effect)
	{
		auto* inv = a_actor ? a_actor->inventoryList : nullptr;
		if (!inv) {
			return Condition::INVALID_HEALTH;
		}

		// The equipped weapon that takes part and carries health extra data.
		// WearsOut does the filtering: every weapon has a health from the
		// moment it spawns, and WearsOut drops thrown weapons, which sit in a
		// second equip slot. A read lock, because this only reads, and the
		// engine's own walk releases the same lock before the damage formula
		// runs.
		float                    found = Condition::INVALID_HEALTH;
		const RE::BSAutoReadLock l(inv->rwLock);
		inv->ForEachStack(
			[a_weapon](RE::BGSInventoryItem& a_item) {
				return a_item.object && Condition::WearsOut(*a_item.object) &&
				       (!a_weapon || static_cast<const RE::TESForm*>(a_item.object) == a_weapon);
			},
			[&found, a_effect](RE::BGSInventoryItem& a_item, RE::BGSInventoryItem::Stack& a_stack) {
				// WearsOut has already made sure this is a weapon.
				if (a_stack.IsEquipped() && a_stack.extra && a_stack.extra->HasType<RE::ExtraHealth>() &&
					(!a_effect || Casts(static_cast<const RE::TESObjectWEAP&>(*a_item.object), a_stack, *a_effect))) {
					found = a_stack.extra->GetHealthPerc();
					return false;  // Stop iteration.
				}
				return true;  // Continue searching...
			});
		return found;
	}

	std::optional<float> TryWeaponHealth(RE::Actor* a_actor, const RE::TESForm* a_weapon)
	{
		auto* inv = a_actor ? a_actor->inventoryList : nullptr;
		if (!inv) {
			return Condition::INVALID_HEALTH;
		}
		if (!inv->rwLock.try_lock_write()) {
			return std::nullopt;
		}

		// The walk's own read lock is allowed inside the write lock, since the
		// lock knows this thread already holds it for writing.
		const auto health = WeaponHealth(a_actor, a_weapon);
		inv->rwLock.unlock_write();
		return health;
	}

	RE::TESObjectWEAP* Weapon(RE::Actor* a_actor)
	{
		auto* inv = a_actor ? a_actor->inventoryList : nullptr;
		if (!inv) {
			return nullptr;
		}

		// The same walk and filter as WeaponHealth: WearsOut keeps a carried
		// grenade, in an equip slot of its own, from being picked ahead of the
		// weapon in hand.
		RE::TESObjectWEAP*       found = nullptr;
		const RE::BSAutoReadLock l(inv->rwLock);
		inv->ForEachStack(
			[](RE::BGSInventoryItem& a_item) {
				return a_item.object && Condition::WearsOut(*a_item.object);
			},
			[&found](RE::BGSInventoryItem& a_item, RE::BGSInventoryItem::Stack& a_stack) {
				if (a_stack.IsEquipped()) {
					// WearsOut has already made sure this is a weapon.
					found = static_cast<RE::TESObjectWEAP*>(a_item.object);
					return false;  // Stop iteration.
				}
				return true;  // Continue searching...
			});
		return found;
	}
}
