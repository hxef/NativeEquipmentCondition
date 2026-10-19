#include "Condition/Equipped.h"

#include "Condition/ArmorWear/ArmorWear.h"

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

		// Every equipped stack of every piece of armor a_takes picks, see
		// ArmorPieces, or every stack with a_carried, in power armor too.
		template <class Takes>
		std::vector<ArmorPiece> Pieces(RE::Actor* a_actor, Takes a_takes, bool a_carried = false)
		{
			std::vector<ArmorPiece> found;
			auto*                   inv = a_actor ? a_actor->inventoryList : nullptr;
			if (!inv || (!a_carried && RE::PowerArmor::ActorInPowerArmor(*a_actor))) {
				return found;
			}

			// A read lock, as WeaponHealth takes.
			const RE::BSAutoReadLock l(inv->rwLock);
			inv->ForEachStack(
				[&a_takes](RE::BGSInventoryItem& a_item) {
					return a_item.object && a_item.object->Is(RE::ENUM_FORM_ID::kARMO) &&
						   a_takes(static_cast<const RE::TESObjectARMO&>(*a_item.object));
				},
				[&found, a_actor, a_carried](RE::BGSInventoryItem& a_item, RE::BGSInventoryItem::Stack& a_stack) {
					if (a_carried || a_stack.IsEquipped()) {
						// The filter has already made sure this is armor.
						auto* armor = static_cast<RE::TESObjectARMO*>(a_item.object);
						found.push_back({ armor, Condition::HealthOf(&a_stack), armor->bipedModelData.bipedObjectSlots,
							Issued(*a_actor, *armor, a_stack.extra.get()), a_stack.count });
					}
					return true;  // Continue searching...
				});
			return found;
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

	std::optional<HeldWeapon> TryHeldWeapon(RE::Actor* a_actor)
	{
		auto* inv = a_actor ? a_actor->inventoryList : nullptr;
		if (!inv) {
			return HeldWeapon{};
		}
		if (!inv->rwLock.try_lock_write()) {
			return std::nullopt;
		}

		// Both walks take the read lock inside this write lock, see
		// TryWeaponHealth.
		HeldWeapon held;
		held.weapon = Weapon(a_actor);
		if (held.weapon) {
			held.health = WeaponHealth(a_actor, held.weapon);
		}
		inv->rwLock.unlock_write();
		return held;
	}

	std::vector<ArmorPiece> ArmorPieces(RE::Actor* a_actor)
	{
		return Pieces(a_actor, Condition::WearsOut);
	}

	std::vector<ArmorPiece> LeftoverPieces(RE::Actor* a_actor)
	{
		const auto takes = [](const RE::TESObjectARMO& a_armor) { return !Condition::WearsOut(a_armor) && !ArmorWear::IsPowerArmor(a_armor); };
		auto       found = Pieces(a_actor, takes, true);
		// -1 is a piece with no health, which stays as it is.
		std::erase_if(found, [](const ArmorPiece& a_piece) { return a_piece.health < 0.0F; });
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

		// The walk's read lock inside this write lock is fine, see
		// TryWeaponHealth.
		auto pieces = ArmorPieces(a_actor);
		inv->rwLock.unlock_write();
		return pieces;
	}

	bool Issued(const RE::Actor& a_holder, const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra)
	{
		// The game tags each item an outfit puts on with that outfit, and the
		// tag survives saving and follows the item into any inventory, so it
		// has to name the holder's own outfit. An item from a plain line of
		// the record's own inventory has no tag, so its kind is matched
		// instead. One from a leveled line carries that list's form ID, which
		// the game takes off once the player gets the item. None has an
		// owner, and whatever the player hands a companion has one, the
		// player or whoever it was stolen from.
		//
		// A character who takes their inventory from a template needs no walk
		// here. The game copies the template's lines and outfits onto the
		// record at load, and onto the copy it makes of a leveled character
		// as it spawns, and fills the inventory from that record.
		const auto* npc = a_holder.GetNPC();
		if (!npc || (a_extra && a_extra->HasType(RE::EXTRA_DATA_TYPE::kOwnership))) {
			return false;
		}

		if (const auto* stamp = a_extra ? a_extra->GetByType<RE::ExtraOutfitItem>() : nullptr) {
			const auto mine = [stamp](const RE::BGSOutfit* a_outfit) { return a_outfit && a_outfit->formID == stamp->outfit; };
			return mine(npc->defOutfit) || mine(npc->sleepOutfit);
		}

		const auto*     listed = a_extra ? a_extra->GetByType<RE::ExtraLeveledItem>() : nullptr;
		const std::span lines{ npc->containerObjects, npc->numContainerObjects };
		return std::ranges::any_of(lines, [&a_object, listed](const RE::ContainerObject* a_line) {
			return a_line && a_line->obj && (a_line->obj == &a_object || (listed && a_line->obj->formID == listed->levItem));
		});
	}

	bool EquippedStack::CompareData(const RE::BGSInventoryItem::Stack& a_stack)
	{
		if (count != 0 || !a_stack.IsEquipped() || !a_stack.extra) {
			return false;
		}
		count = a_stack.count;
		return true;
	}

	void WearStack::WriteDataImpl(RE::TESBoundObject& a_object, RE::BGSInventoryItem::Stack& a_stack)
	{
		if (!a_stack.extra) {
			return;
		}

		moved = Condition::Decrease(*a_stack.extra, a_object, amount, source, scale, theirs);

		// The engine's own broken check, run on the health just written. It
		// is the same check that stops the item going back on, see MIN_HEALTH
		// in Condition.h.
		broke = moved && a_stack.extra->IsItemBroken();
	}

	void WriteStack(RE::TESObjectREFR& a_owner, RE::TESBoundObject& a_object,
		RE::BGSInventoryItem::StackDataCompareFunctor& a_find, RE::BGSInventoryItem::StackDataWriteFunctor& a_write)
	{
		// The equipped flags move onto the split off copy, so the item in use
		// and the item written stay the same one.
		a_write.transferEquippedToSplitStack = true;
		a_owner.FindAndWriteStackDataForInventoryItem(&a_object, a_find, a_write);
	}
}
