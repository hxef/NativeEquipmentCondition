#include "Condition/Equipped.h"

#include <algorithm>
#include <span>

namespace Equipped
{
	namespace
	{
		// Whether this copy of a weapon casts a_effect, as its own object
		// effect or one a mod adds.
		bool Casts(const RE::TESObjectWEAP& a_weapon, const RE::BGSInventoryItem::Stack& a_stack,
			const RE::EnchantmentItem& a_effect)
		{
			if (a_weapon.GetBaseEnchanting() == &a_effect) {
				return true;
			}

			const auto& data = Condition::InstanceDataOf<RE::TESObjectWEAP::InstanceData>(a_stack.extra.get(), a_weapon.weaponData);
			const auto* effects = data.GetEnchantmentArray();
			return effects && std::ranges::find(*effects, &a_effect) != effects->end();
		}

		// Whether this copy of a piece is part of the wearer's own outfit from
		// the editor. The game tags each piece an outfit puts on with that
		// outfit, and the tag survives saving and follows the piece into any
		// inventory, so it has to name the wearer's own outfit. A piece from a
		// line of the record's own inventory has no tag, so its kind is matched
		// instead. Neither has an owner, and whatever the player hands a
		// companion has one, the player or whoever it was stolen from, so a
		// copy handed over never counts.
		bool Issued(const RE::Actor& a_wearer, const RE::TESObjectARMO& a_armor, const RE::ExtraDataList* a_extra)
		{
			const auto* npc = a_wearer.GetNPC();
			if (!npc || (a_extra && a_extra->HasType(RE::EXTRA_DATA_TYPE::kOwnership))) {
				return false;
			}

			if (const auto* stamp = a_extra ? a_extra->GetByType<RE::ExtraOutfitItem>() : nullptr) {
				const auto mine = [stamp](const RE::BGSOutfit* a_outfit) { return a_outfit && a_outfit->formID == stamp->outfit; };
				return mine(npc->defOutfit) || mine(npc->sleepOutfit);
			}

			const std::span lines{ npc->containerObjects, npc->numContainerObjects };
			return std::ranges::any_of(lines, [&a_armor](const RE::ContainerObject* a_line) { return a_line && a_line->obj == &a_armor; });
		}
	}

	float WeaponHealth(RE::Actor* a_actor, const RE::TESForm* a_weapon, const RE::EnchantmentItem* a_effect)
	{
		auto* inv = a_actor ? a_actor->inventoryList : nullptr;
		if (!inv) {
			return Condition::INVALID_HEALTH;
		}

		// The equipped weapon that takes part. IsWeapon keeps armor out, since
		// a chest piece takes part too and is equipped the same way. WearsOut
		// does the rest, and drops thrown weapons, which sit in a second equip
		// slot. A read lock, because this only reads, and the engine's own
		// walk releases the same lock before the damage formula runs.
		float                    found = Condition::INVALID_HEALTH;
		const RE::BSAutoReadLock l(inv->rwLock);
		inv->ForEachStack(
			[a_weapon](RE::BGSInventoryItem& a_item) {
				return a_item.object && a_item.object->IsWeapon() && Condition::WearsOut(*a_item.object) &&
					   (!a_weapon || static_cast<const RE::TESForm*>(a_item.object) == a_weapon);
			},
			[&found, a_effect](RE::BGSInventoryItem& a_item, RE::BGSInventoryItem::Stack& a_stack) {
				// WearsOut has already made sure this is a weapon.
				if (a_stack.IsEquipped() &&
					(!a_effect || Casts(static_cast<const RE::TESObjectWEAP&>(*a_item.object), a_stack, *a_effect))) {
					const auto health = Condition::HealthOf(&a_stack);
					found = health < 0.0F ? Condition::MAX_HEALTH : health;
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
		// weapon in hand, and IsWeapon keeps armor out.
		RE::TESObjectWEAP*       found = nullptr;
		const RE::BSAutoReadLock l(inv->rwLock);
		inv->ForEachStack(
			[](RE::BGSInventoryItem& a_item) {
				return a_item.object && a_item.object->IsWeapon() && Condition::WearsOut(*a_item.object);
			},
			[&found](RE::BGSInventoryItem& a_item, RE::BGSInventoryItem::Stack& a_stack) {
				if (a_stack.IsEquipped()) {
					// The filter has already made sure this is a weapon.
					found = static_cast<RE::TESObjectWEAP*>(a_item.object);
					return false;  // Stop iteration.
				}
				return true;  // Continue searching...
			});
		return found;
	}

	std::vector<ArmorPiece> ArmorPieces(RE::Actor* a_actor)
	{
		std::vector<ArmorPiece> found;
		auto*                   inv = a_actor ? a_actor->inventoryList : nullptr;
		if (!inv || RE::PowerArmor::ActorInPowerArmor(*a_actor)) {
			return found;
		}

		// Every equipped stack of every piece that takes part. A read lock,
		// as WeaponHealth takes.
		const RE::BSAutoReadLock l(inv->rwLock);
		inv->ForEachStack(
			[](RE::BGSInventoryItem& a_item) {
				return a_item.object && a_item.object->Is(RE::ENUM_FORM_ID::kARMO) && Condition::WearsOut(*a_item.object);
			},
			[&found, a_actor](RE::BGSInventoryItem& a_item, RE::BGSInventoryItem::Stack& a_stack) {
				if (a_stack.IsEquipped()) {
					// The filter has already made sure this is armor.
					auto* armor = static_cast<RE::TESObjectARMO*>(a_item.object);
					found.push_back({ armor, Condition::HealthOf(&a_stack), armor->bipedModelData.bipedObjectSlots,
						Issued(*a_actor, *armor, a_stack.extra.get()) });
				}
				return true;  // Continue searching...
			});
		return found;
	}

	std::optional<std::vector<ArmorPiece>> TryArmorPieces(RE::Actor* a_actor)
	{
		auto* inv = a_actor ? a_actor->inventoryList : nullptr;
		if (!inv) {
			return std::vector<ArmorPiece>{};
		}
		if (!inv->rwLock.try_lock_write()) {
			return std::nullopt;
		}

		// The walk's read lock inside this write lock is fine, see TryWeaponHealth.
		auto pieces = ArmorPieces(a_actor);
		inv->rwLock.unlock_write();
		return pieces;
	}

	bool EquippedStack::CompareData(const RE::BGSInventoryItem::Stack& a_stack)
	{
		if (!a_stack.IsEquipped() || !a_stack.extra) {
			return false;
		}
		count = a_stack.count;
		return true;
	}

	void WriteStack(RE::TESObjectREFR& a_owner, RE::TESBoundObject& a_object,
		RE::BGSInventoryItem::StackDataCompareFunctor& a_find, RE::BGSInventoryItem::StackDataWriteFunctor& a_write)
	{
		// The equipped flags move onto the split off copy, so the item in use
		// and the item written stay the same one.
		a_write.transferEquippedToSplitStack = true;
		a_owner.FindAndWriteStackDataForInventoryItem(&a_object, a_find, a_write);
	}

	std::uint32_t WriteEquipped(RE::TESObjectREFR& a_owner, RE::TESBoundObject& a_object,
		RE::BGSInventoryItem::StackDataWriteFunctor& a_write)
	{
		EquippedStack equipped;
		WriteStack(a_owner, a_object, equipped, a_write);
		return equipped.count;
	}
}
