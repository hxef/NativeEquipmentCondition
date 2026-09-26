#include "UI/Inventory/ItemCard/Cards.h"

#include "Condition/Condition.h"
#include "Core/CallPatch.h"
#include "Core/TraceLog.h"
#include "Gameplay/FireRate.h"
#include "UI/Inventory/Pipboy.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

namespace ItemCard
{
	namespace
	{
		using CallPatch::CallSite;
		using Scaleform::GFx::Value;

		// The equipped items a card is compared with, each an item and the
		// stack of it that is equipped.
		using CompareItems = RE::BSScrapArray<RE::BSTTuple<const RE::BGSInventoryItem*, std::uint32_t>>;

		// The calls to InventoryUserUIUtils::PopulateItemCardInfo_Helper, which
		// builds the card for every menu except the Pip-Boy.
		constexpr CallSite HELPER_SITES[] = {
			// InventoryUserUIUtils::PopulateItemCardInfo, for containers and
			// bartering
			{ 2222624, 0x0E6, "container card" },
			// ExamineMenu::UpdateItemCard, for the workbench, inspecting an
			// item and the power armor station
			{ 2223053, 0x281, "examine card" },
			// CookingMenu::UpdateItemCard
			{ 2222904, 0x0E9, "cooking card" },
		};

		// The calls to PipboyInventoryData::PopulateItemCardInfo, the same job
		// for the Pip-Boy, a card for every item it lists.
		constexpr CallSite PIPBOY_SITES[] = {
			// PipboyInventoryData::InitializeItem, when an item first appears
			// in the Pip-Boy. A tail call, see CallPatch::PatchCall, whose
			// return value nothing reads.
			{ RE::ID::PipboyInventoryData::InitializeItem.id(), 0x651, "pipboy card" },
			// PipboyInventoryData::RepopulateItemCardOnSection, when a category
			// of cards is rebuilt, which ItemCards::Refresh asks for after wear
			{ RE::ID::PipboyInventoryData::RepopulateItemCardOnSection.id(), 0x40F, "pipboy card rebuild" },
		};

		// The calls to CombatFormulas::GetWeaponDisplayRateOfFire inside the 2
		// functions above. The game hands them the weapon and its mods and
		// nothing saying which copy, so the hooks read that from Building.
		constexpr CallSite RATE_SITES[] = {
			// PopulateItemCardInfo_Helper, for the item on the card
			{ RE::ID::InventoryUserUIUtils::PopulateItemCardInfo_Helper.id(), 0x7E7, "card fire rate" },
			// PopulateItemCardInfo_Helper again, for the equipped weapon the
			// item is compared with. The card shows the difference between the
			// 2.
			{ RE::ID::InventoryUserUIUtils::PopulateItemCardInfo_Helper.id(), 0x7FF, "card fire rate compared" },
			// PipboyInventoryData::PopulateItemCardInfo, which compares nothing
			{ RE::ID::PipboyInventoryData::PopulateItemCardInfo.id(), 0x454, "pipboy card fire rate" },
		};

		// 2 lists compare weapons by fire rate without a card. Each reads a
		// weapon's stack just before asking for the rate, so hooks on those
		// calls note the stack's condition for the rate hook, see t_listHealth.
		// The first is the check whether an item beats the equipped weapon, for
		// the quick container's better mark, which adds up damage types through
		// PipboyInventoryUtils::FillDamageTypeInfo and multiplies by the rate.
		constexpr CallSite BETTER_TYPES_SITES[] = {
			{ 2222626, 0x198, "better check types" },
			{ 2222626, 0x210, "better check equipped types" },
		};
		constexpr CallSite BETTER_RATE_SITES[] = {
			{ 2222626, 0x1CF, "better check fire rate" },
			{ 2222626, 0x242, "better check equipped fire rate" },
		};

		// The second is the comparator a container or a trader sorts with. By
		// fire rate, it reads each weapon's mods through
		// BGSInventoryItem::GetInstanceData and asks for the rate right after.
		// The Pip-Boy sorts by the rates on its cards.
		constexpr CallSite SORT_MODS_SITE{ 2222850, 0x2EB, "sort mods" };
		constexpr CallSite SORT_RATE_SITE{ 2222850, 0x33E, "sort fire rate" };

		// The card the game is building on this thread, set while the game's
		// own builder runs.
		struct Building
		{
			// The condition of the item on the card.
			float health;

			// The equipped items it is compared with. The Pip-Boy has none.
			const CompareItems* compare;
		};

		thread_local const Building* t_building = nullptr;

		// The condition of the stack a list asks the fire rate of next, noted
		// by the call just before on the same thread. A list's note hooks and
		// rate hooks go in together or not at all.
		thread_local float t_listHealth = Condition::INVALID_HEALTH;

		// Which of the 2 lists noted it, for the trace.
		thread_local const char* t_list = nullptr;

		// Moves the entry just appended to the front. The card turns entries
		// into rows from last to first, so the first entry becomes the highest
		// plain row, directly under Damage, which is where it stays in a menu
		// the render listener in Raise.cpp cannot reach.
		void MoveLastToFront(Value& a_entries)
		{
			const auto size = a_entries.GetArraySize();
			if (size < 2) {
				return;
			}

			Value last;
			a_entries.GetElement(size - 1, &last);
			for (auto i = size - 1; i > 0; i--) {
				Value previous;
				a_entries.GetElement(i - 1, &previous);
				a_entries.SetElement(i, previous);
			}
			a_entries.SetElement(0, last);
		}

		// Stands in for InventoryUserUIUtils::PopulateItemCardInfo_Helper. A
		// menu refreshing its card passes the card's array. A menu building its
		// list passes a list entry, which gets the array as its
		// ItemCardInfoList member. a_compareItems and
		// a_compareArmorWeightAndValue go through untouched.
		void PopulateHelperHk(Value& a_target, const RE::BGSInventoryItem& a_item, std::uint32_t a_stackID,
			const CompareItems& a_compareItems, bool a_compareArmorWeightAndValue)
		{
			const auto*    stack = a_item.GetStackByID(a_stackID);
			const Building building{ Condition::HealthOf(stack), &a_compareItems };

			t_building = &building;
			RE::InventoryUserUIUtils::PopulateItemCardInfo_Helper(a_target, a_item, a_stackID, a_compareItems,
				a_compareArmorWeightAndValue);
			t_building = nullptr;

			const auto percent = Condition::Percent(a_item, stack);
			if (!percent) {
				return;
			}

			Value entries;
			if (a_target.IsArray()) {
				entries = a_target;
			} else if (!a_target.GetMember("ItemCardInfoList"sv, &entries) || !entries.IsArray()) {
				return;
			}

			// The engine's own entry builder, so the row carries every field
			// the card reads. showAsPercent prints 91 as 91%.
			Value entry;
			RE::InventoryUserUIUtils::AddItemCardInfoEntry(entries, entry, CND_TEXT, Value(*percent));
			entry.SetMember("showAsPercent"sv, Value(true));
			MoveLastToFront(entries);
		}

		// Stands in for PipboyInventoryData::PopulateItemCardInfo. The Pip-Boy
		// keeps its cards as a tree of values of its own, copied into Scaleform
		// objects when a page shows a card, so the row is built from those
		// values.
		void PipboyPopulateHk(RE::PipboyInventoryData* a_this, const RE::BGSInventoryItem* a_item,
			const RE::BGSInventoryItem::Stack* a_stack, RE::PipboyObject* a_data)
		{
			const Building building{ Condition::HealthOf(a_stack), nullptr };

			t_building = &building;
			a_this->PopulateItemCardInfo(a_item, a_stack, a_data);
			t_building = nullptr;

			const auto percent = a_item && a_data ? Condition::Percent(*a_item, a_stack) : std::nullopt;
			if (!percent) {
				return;
			}

			// The item's own entry, which the Pip-Boy numbers as it builds it.
			// Pipboy::MarkBroken keeps the number of a worn out one to fade its
			// name. It runs either way, since a repaired item needs its number
			// forgotten.
			Pipboy::MarkBroken(*a_data, a_stack && a_stack->extra && a_stack->extra->IsItemBroken());

			// The call above created this array from scratch, so its order is
			// still free to change.
			auto* entries = a_data->GetMember<RE::PipboyArray*>(RE::BSFixedString("itemCardInfoList"));
			if (!entries) {
				return;
			}

			const RE::BSFixedStringCS text{ CND_TEXT };
			a_this->AddItemCardInfoEntry(&text, static_cast<float>(*percent), entries);

			// The helper appends the entry without handing it back.
			auto& elements = entries->elements;
			if (elements.empty()) {
				return;
			}
			auto* entry = static_cast<RE::PipboyObject*>(elements.back());

			const RE::BSFixedString showAsPercent{ "showAsPercent" };
			entry->AddMember(&showAsPercent, new RE::PipboyPrimitiveValue<bool>(true, entry));

			std::rotate(elements.begin(), elements.end() - 1, elements.end());
		}

		// PipboyPopulateHk where the Pip-Boy lists an item it has not listed
		// before. The trace names every new entry of an item that wears, which
		// shows a split stack reached the Pip-Boy as rows of its own.
		void PipboyNewEntryHk(RE::PipboyInventoryData* a_this, const RE::BGSInventoryItem* a_item,
			const RE::BGSInventoryItem::Stack* a_stack, RE::PipboyObject* a_data)
		{
			PipboyPopulateHk(a_this, a_item, a_stack, a_data);

			if (!TraceLog::IsOpen() || !a_item || !a_item->object || !a_stack) {
				return;
			}
			const auto percent = Condition::Percent(*a_item, a_stack);
			if (percent) {
				TraceLog::Line("menu", "Pip-Boy lists {:s} [{:08X}] x{:d} at {:.0f}%",
					RE::TESFullName::GetFullName(*a_item->object), a_item->object->formID, a_stack->GetCount(), *percent);
			}
		}

		// The fire rate a card prints for a copy in this condition: the game's
		// own figure at the share the copy fires at, see FireRate.h.
		float RateAt(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data, float a_health)
		{
			return RE::CombatFormulas::GetWeaponDisplayRateOfFire(a_weapon, a_data) *
			       FireRate::RateShare(a_weapon, a_data, a_health);
		}

		// Stands in for CombatFormulas::GetWeaponDisplayRateOfFire where a card
		// works out the fire rate of the item it is about.
		float CardRateHk(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data)
		{
			return RateAt(a_weapon, a_data, t_building ? t_building->health : Condition::INVALID_HEALTH);
		}

		// Stands in for it where the card works out the rate of the equipped
		// weapon the item is compared with. Only one copy of a weapon can be
		// equipped, so its condition is that copy's.
		float ComparedRateHk(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data)
		{
			auto health = Condition::INVALID_HEALTH;
			if (t_building && t_building->compare) {
				for (const auto& equipped : *t_building->compare) {
					if (equipped.first && equipped.first->object == &a_weapon) {
						health = Condition::HealthOf(equipped.first->GetStackByID(equipped.second));
						break;
					}
				}
			}
			return RateAt(a_weapon, a_data, health);
		}

		// Stands in for PipboyInventoryUtils::FillDamageTypeInfo where the
		// better check adds up a weapon's damage types.
		void BetterTypesHk(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack,
			RE::BSScrapArray<RE::BSTTuple<std::uint32_t, float>>& a_damageValuesPerType)
		{
			t_listHealth = Condition::HealthOf(a_stack);
			t_list = "The better check";
			RE::PipboyInventoryUtils::FillDamageTypeInfo(a_item, a_stack, a_damageValuesPerType);
		}

		// Stands in for BGSInventoryItem::GetInstanceData where the sort reads
		// a weapon's mods.
		RE::TBO_InstanceData* SortModsHk(const RE::BGSInventoryItem& a_item, std::uint32_t a_stackID)
		{
			t_listHealth = Condition::HealthOf(a_item.GetStackByID(a_stackID));
			t_list = "The sort";
			return a_item.GetInstanceData(a_stackID);
		}

		// Stands in for CombatFormulas::GetWeaponDisplayRateOfFire in both
		// lists.
		float ListRateHk(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data)
		{
			const auto rate = RateAt(a_weapon, a_data, t_listHealth);

			// A worn automatic compared at less than its full rate is what the
			// hooks are for, so that is what the trace says. A sort asks about
			// the same gun many times, hence Once.
			if (TraceLog::IsOpen()) {
				const auto share = FireRate::RateShare(a_weapon, a_data, t_listHealth);
				if (share < 1.0F) {
					TraceLog::Once("menu", "{:s} weighs {:s} [{:08X}] at condition {:.3f} at a card rate of {:.2f}, {:.3f} of its own",
						t_list ? t_list : "A list", RE::TESFullName::GetFullName(a_weapon), a_weapon.formID, t_listHealth, rate, share);
				}
			}
			return rate;
		}
	}

	bool PatchCards()
	{
		const auto helper = CallPatch::PatchAll(HELPER_SITES, RE::ID::InventoryUserUIUtils::PopulateItemCardInfo_Helper,
			CallPatch::Repeat<std::size(HELPER_SITES)>(reinterpret_cast<std::uintptr_t>(&PopulateHelperHk)),
			"Menu item cards show a CND row");

		const auto pipboy = CallPatch::PatchAll(PIPBOY_SITES, RE::ID::PipboyInventoryData::PopulateItemCardInfo,
			std::array{ reinterpret_cast<std::uintptr_t>(&PipboyNewEntryHk), reinterpret_cast<std::uintptr_t>(&PipboyPopulateHk) },
			"Pip-Boy item cards show a CND row");

		const bool patched = helper > 0 || pipboy > 0;

		// While a worn gun fires as fast as a new one, the game's own fire rate
		// is the true one, so the menus keep it.
		if (!FireRate::Slows()) {
			return patched;
		}

		// Each set goes in whole or not at all, so every card prints the same
		// rate for the same gun and a list's rate hook always has its note.
		const auto rate = RE::ID::CombatFormulas::GetWeaponDisplayRateOfFire.address();
		const auto cardRate = reinterpret_cast<std::uintptr_t>(&CardRateHk);
		const auto listRate = reinterpret_cast<std::uintptr_t>(&ListRateHk);

		const auto cards = CallPatch::PatchTogether({
			{ RATE_SITES[0], rate, cardRate },
			{ RATE_SITES[1], rate, reinterpret_cast<std::uintptr_t>(&ComparedRateHk) },
			{ RATE_SITES[2], rate, cardRate },
		});
		if (cards) {
			REX::INFO("Item cards print the fire rate a worn gun fires at.");
		} else {
			REX::ERROR("Item cards will keep printing a worn gun at the fire rate of a new one.");
		}

		const auto types = RE::ID::PipboyInventoryUtils::FillDamageTypeInfo.address();
		const auto betterTypes = reinterpret_cast<std::uintptr_t>(&BetterTypesHk);
		const auto better = CallPatch::PatchTogether({
			{ BETTER_TYPES_SITES[0], types, betterTypes },
			{ BETTER_TYPES_SITES[1], types, betterTypes },
			{ BETTER_RATE_SITES[0], rate, listRate },
			{ BETTER_RATE_SITES[1], rate, listRate },
		});
		if (better) {
			REX::INFO("The quick container weighs a worn gun at the fire rate it fires at before it marks an item better.");
		} else {
			REX::ERROR("The quick container will keep weighing a worn gun at the fire rate of a new one.");
		}

		const auto sort = CallPatch::PatchTogether({
			{ SORT_MODS_SITE, RE::ID::BGSInventoryItem::GetInstanceData.address(), reinterpret_cast<std::uintptr_t>(&SortModsHk) },
			{ SORT_RATE_SITE, rate, listRate },
		});
		if (sort) {
			REX::INFO("Containers and traders sort a worn gun by the fire rate it fires at.");
		} else {
			REX::ERROR("Containers and traders will keep sorting a worn gun by the fire rate of a new one.");
		}

		return patched;
	}
}
