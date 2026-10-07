#include "UI/Inventory/ItemCard/Cards.h"

#include "Condition/Condition.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/TraceLog.h"
#include "Gameplay/FireRate/FireRate.h"
#include "UI/Inventory/Pipboy.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace ItemCard
{
	namespace
	{
		using CallPatch::CallSite;
		using Scaleform::GFx::Value;

		// The equipped items a card is compared with, each with its stack.
		using CompareItems = RE::BSScrapArray<RE::BSTTuple<const RE::BGSInventoryItem*, std::uint32_t>>;

		// The calls to InventoryUserUIUtils::PopulateItemCardInfo_Helper, which
		// builds the card for every menu except the Pip-Boy: containers and
		// bartering, the workbench and inspect and the power armor station,
		// and cooking.
		constexpr CallSite HELPER_SITES[] = {
			{ 2222624, 0x0E6, "container card" },
			{ 2223053, 0x281, "examine card" },
			{ 2222904, 0x0E9, "cooking card" },
		};

		// The calls to PipboyInventoryData::PopulateItemCardInfo, the same job
		// for the Pip-Boy. The first, in InitializeItem, is a tail call whose
		// return nothing reads, see PatchCall. It runs as the game lists an
		// item, which it does again for a gun after every shot. The second is
		// a category rebuilt, on an equip and when ItemCards::Refresh asks
		// after wear.
		constexpr CallSite PIPBOY_SITES[] = {
			{ RE::ID::PipboyInventoryData::InitializeItem.id(), 0x651, "pipboy card", true },
			{ RE::ID::PipboyInventoryData::RepopulateItemCardOnSection.id(), 0x40F, "pipboy card rebuild" },
		};

		// The calls to CombatFormulas::GetWeaponDisplayRateOfFire inside the 2
		// functions above. The game hands them the weapon and its mods, not
		// which copy, so the hooks read that from Building.
		constexpr CallSite RATE_SITES[] = {
			// PopulateItemCardInfo_Helper, the item on the card and the
			// equipped weapon it is compared with, then the Pip-Boy's card.
			{ RE::ID::InventoryUserUIUtils::PopulateItemCardInfo_Helper.id(), 0x7E7, "card fire rate" },
			{ RE::ID::InventoryUserUIUtils::PopulateItemCardInfo_Helper.id(), 0x7FF, "card fire rate compared" },
			{ RE::ID::PipboyInventoryData::PopulateItemCardInfo.id(), 0x454, "pipboy card fire rate" },
		};

		// 2 lists compare weapons by fire rate without a card. Each reads a
		// weapon's stack just before asking for the rate, so hooks on those
		// calls note the stack's condition for the rate hook, see t_listHealth.
		// The first is the better mark of the quick container, which adds up
		// damage types through FillDamageTypeInfo and multiplies by the rate.
		constexpr CallSite BETTER_TYPES_SITES[] = {
			{ 2222626, 0x198, "better check types" },
			{ 2222626, 0x210, "better check equipped types" },
		};
		constexpr CallSite BETTER_RATE_SITES[] = {
			{ 2222626, 0x1CF, "better check fire rate" },
			{ 2222626, 0x242, "better check equipped fire rate" },
		};

		// The second is the sort a container or a trader uses. By fire rate it
		// reads each weapon's mods through GetInstanceData and asks for the
		// rate right after. The Pip-Boy sorts by the rates on its cards.
		constexpr CallSite SORT_MODS_SITE{ 2222850, 0x2EB, "sort mods" };
		constexpr CallSite SORT_RATE_SITE{ 2222850, 0x33E, "sort fire rate" };

		// The card the game is building on this thread, set while the game's own
		// builder runs: the condition of the item on it, and the equipped
		// items it is compared with, none for the Pip-Boy.
		struct Building
		{
			float               health;
			const CompareItems* compare;
		};

		thread_local const Building* t_building = nullptr;

		// The 2 Pip-Boy sites, and the rate hooks of the cards. A list that
		// weighs fire rates keeps its name for the trace and its own Held for
		// its note and rate hooks.
		CallPatch::Held g_pipboy;
		CallPatch::Held g_cards;
		struct List
		{
			const char*     name;
			CallPatch::Held held;
		};

		List g_better{ "The better check", {} };
		List g_sort{ "The sort", {} };

		// The condition of the stack a list asks the rate of next, and the list.
		thread_local float       t_listHealth = Condition::INVALID_HEALTH;
		thread_local const List* t_list = nullptr;

		// The rate Link's type serves the card, compared and list rate sites
		// alike.
		using Helper_t = void (*)(Scaleform::GFx::Value&, const RE::BGSInventoryItem&, std::uint32_t, const CompareItems&, bool);
		using Pipboy_t = void (*)(RE::PipboyInventoryData*, const RE::BGSInventoryItem*, const RE::BGSInventoryItem::Stack*, RE::PipboyObject*);
		using Rate_t = float (*)(const RE::TESObjectWEAP&, const RE::TESObjectWEAP::InstanceData*);
		using Types_t = void (*)(const RE::BGSInventoryItem&, const RE::BGSInventoryItem::Stack*, RE::BSScrapArray<RE::BSTTuple<std::uint32_t, float>>&);
		using Mods_t = RE::TBO_InstanceData* (*)(const RE::BGSInventoryItem&, std::uint32_t);

		std::array<CallPatch::Link<Helper_t>, std::size(HELPER_SITES)> g_helperLinks;
		std::array<CallPatch::Link<Pipboy_t>, std::size(PIPBOY_SITES)> g_pipboyLinks;
		std::array<CallPatch::Link<Rate_t>, std::size(RATE_SITES)>     g_cardLinks;
		std::array<CallPatch::Link<Types_t>, std::size(BETTER_TYPES_SITES)> g_betterTypeLinks;
		std::array<CallPatch::Link<Rate_t>, 3>                        g_listLinks;  // better rate x2, sort rate
		CallPatch::Link<Mods_t>                                       g_sortModsLink;

		// Moves the entry just appended to the front. The card turns entries
		// into rows from last to first, so the first entry becomes the highest
		// plain row, directly under Damage, where it stays in a menu the
		// render listener in Raise.cpp cannot reach.
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
		// menu refreshing its card passes the card's array. A menu building
		// its list passes a list entry, which gets the array as its
		// ItemCardInfoList member. The last 2 arguments go through untouched.
		template <std::size_t I>
		void PopulateHelperHk(Value& a_target, const RE::BGSInventoryItem& a_item, std::uint32_t a_stackID,
			const CompareItems& a_compareItems, bool a_compareArmorWeightAndValue)
		{
			const auto*    stack = a_item.GetStackByID(a_stackID);
			const Building building{ Condition::HealthOf(stack), &a_compareItems };

			t_building = g_helperLinks[I].Live() ? &building : nullptr;
			g_helperLinks[I](a_target, a_item, a_stackID, a_compareItems, a_compareArmorWeightAndValue);
			const auto live = std::exchange(t_building, nullptr) != nullptr;

			const auto percent = live ? Condition::Percent(a_item, stack) : std::nullopt;
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
		// objects when a page shows a card, so the row is built from those.
		// a_link is the calling site's.
		//
		// Both sites build the same card, so while either is left to another
		// mod NEC leaves the card as the game builds it: no CND row, the fire
		// rate of an unworn gun and no faded name. With 1 site the row would
		// come and go.
		void PopulatePipboy(RE::PipboyInventoryData* a_this, const RE::BGSInventoryItem* a_item,
			const RE::BGSInventoryItem::Stack* a_stack, RE::PipboyObject* a_data, const CallPatch::Link<Pipboy_t>& a_link)
		{
			const Building building{ Condition::HealthOf(a_stack), nullptr };

			t_building = g_pipboy.Runs(a_link) ? &building : nullptr;
			a_link(a_this, a_item, a_stack, a_data);
			const auto live = std::exchange(t_building, nullptr) != nullptr;

			const auto percent = live && a_item && a_data ? Condition::Percent(*a_item, a_stack) : std::nullopt;
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

		// PopulatePipboy for the first site, where the Pip-Boy lists an item.
		// The trace names each entry of an item that wears, which shows a
		// split stack reached the Pip-Boy as its own rows.
		void PipboyNewEntryHk(RE::PipboyInventoryData* a_this, const RE::BGSInventoryItem* a_item,
			const RE::BGSInventoryItem::Stack* a_stack, RE::PipboyObject* a_data)
		{
			PopulatePipboy(a_this, a_item, a_stack, a_data, g_pipboyLinks[0]);

			if (!TraceLog::IsOpen() || !a_item || !a_item->object || !a_stack) {
				return;
			}
			const auto percent = Condition::Percent(*a_item, a_stack);
			if (percent) {
				TraceLog::Line("menu", "Pip-Boy lists {:s} [{:08X}] x{:d} at {:.0f}%",
					RE::TESFullName::GetFullName(*a_item->object), a_item->object->formID, a_stack->GetCount(), *percent);
			}
		}

		// The second site, where a card is rebuilt.
		void PipboyPopulateHk(RE::PipboyInventoryData* a_this, const RE::BGSInventoryItem* a_item,
			const RE::BGSInventoryItem::Stack* a_stack, RE::PipboyObject* a_data)
		{
			PopulatePipboy(a_this, a_item, a_stack, a_data, g_pipboyLinks[1]);
		}

		// The fire rate a card prints for a copy in this condition: the game's
		// own figure at the share the copy fires at, see FireRate.h.
		float RateAt(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data, float a_health,
			const CallPatch::Link<Rate_t>& a_link)
		{
			return a_link(a_weapon, a_data) * FireRate::RateShare(a_weapon, a_data, a_health);
		}

		// Stands in for CombatFormulas::GetWeaponDisplayRateOfFire where a card
		// works out the fire rate of the item it is about. One per rate site.
		template <std::size_t I>
		float CardRateHk(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data)
		{
			if (!g_cards.Runs(g_cardLinks[I])) {
				return g_cardLinks[I](a_weapon, a_data);
			}
			return RateAt(a_weapon, a_data, t_building ? t_building->health : Condition::INVALID_HEALTH, g_cardLinks[I]);
		}

		// Stands in for it where the card works out the rate of the equipped
		// weapon the item is compared with. Only one copy of a weapon can be
		// equipped, so its condition is that copy's.
		template <std::size_t I>
		float ComparedRateHk(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data)
		{
			if (!g_cards.Runs(g_cardLinks[I])) {
				return g_cardLinks[I](a_weapon, a_data);
			}
			auto health = Condition::INVALID_HEALTH;
			if (t_building && t_building->compare) {
				for (const auto& equipped : *t_building->compare) {
					if (equipped.first && equipped.first->object == &a_weapon) {
						health = Condition::HealthOf(equipped.first->GetStackByID(equipped.second));
						break;
					}
				}
			}
			return RateAt(a_weapon, a_data, health, g_cardLinks[I]);
		}

		// Stands in for PipboyInventoryUtils::FillDamageTypeInfo where the
		// better check adds up a weapon's damage types. One per site.
		template <std::size_t I>
		void BetterTypesHk(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack,
			RE::BSScrapArray<RE::BSTTuple<std::uint32_t, float>>& a_damageValuesPerType)
		{
			if (g_better.held.Runs(g_betterTypeLinks[I])) {
				t_listHealth = Condition::HealthOf(a_stack);
				t_list = &g_better;
			}
			g_betterTypeLinks[I](a_item, a_stack, a_damageValuesPerType);
		}

		// Stands in for BGSInventoryItem::GetInstanceData where the sort reads
		// a weapon's mods.
		RE::TBO_InstanceData* SortModsHk(const RE::BGSInventoryItem& a_item, std::uint32_t a_stackID)
		{
			if (g_sort.held.Runs(g_sortModsLink)) {
				t_listHealth = Condition::HealthOf(a_item.GetStackByID(a_stackID));
				t_list = &g_sort;
			}
			return g_sortModsLink(a_item, a_stackID);
		}

		// Stands in for CombatFormulas::GetWeaponDisplayRateOfFire in both
		// lists, I being 0 or 1 at the better check and 2 at the sort. It asks
		// its own list's set on every call, so a rate place another DLL shares
		// shows its hook ran with no note, and scales only what its list noted.
		template <std::size_t I>
		float ListRateHk(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data)
		{
			// Read once, so a rate asked without its note never takes the
			// condition of the stack before it.
			const auto  health = std::exchange(t_listHealth, Condition::INVALID_HEALTH);
			const auto* list = std::exchange(t_list, nullptr);
			const auto& own = I < 2 ? g_better : g_sort;
			if (!own.held.Runs(g_listLinks[I]) || list != &own) {
				return g_listLinks[I](a_weapon, a_data);
			}
			const auto rate = RateAt(a_weapon, a_data, health, g_listLinks[I]);

			// The trace names a worn automatic weighed below its full rate. A
			// sort asks about the same gun many times, hence Once.
			if (TraceLog::IsOpen()) {
				const auto share = FireRate::RateShare(a_weapon, a_data, health);
				if (share < 1.0F) {
					TraceLog::Once("menu", "{:s} weighs {:s} [{:08X}] at condition {:.3f} at a card rate of {:.2f}, {:.3f} of its own",
						own.name, RE::TESFullName::GetFullName(a_weapon), a_weapon.formID, health, rate, share);
				}
			}
			return rate;
		}
	}

	bool PatchCards()
	{
		const auto helperHooks = CallPatch::PerSite<std::size(HELPER_SITES)>([]<std::size_t I>() { return &PopulateHelperHk<I>; });
		const auto helper = CallPatch::PatchAll(HELPER_SITES, RE::ID::InventoryUserUIUtils::PopulateItemCardInfo_Helper, helperHooks,
			g_helperLinks, "Menu item cards show a CND row");

		// The 2 Pip-Boy sites build the same card, so they go in together, see
		// PopulatePipboy.
		const auto populate = RE::ID::PipboyInventoryData::PopulateItemCardInfo.address();
		g_pipboy = CallPatch::PatchTogether({
			{ PIPBOY_SITES[0], populate, reinterpret_cast<std::uintptr_t>(&PipboyNewEntryHk), &g_pipboyLinks[0] },
			{ PIPBOY_SITES[1], populate, reinterpret_cast<std::uintptr_t>(&PipboyPopulateHk), &g_pipboyLinks[1] },
		});
		if (g_pipboy) {
			REX::INFO("Pip-Boy item cards show a CND row.");
		}

		const bool patched = helper > 0 || static_cast<bool>(g_pipboy);

		// While a worn gun fires as fast as a new one, the game's own fire rate
		// is the true one, so the menus keep it.
		if (!FireRate::Slows()) {
			return patched;
		}

		// Each set goes in whole or not at all, so every card prints the same
		// rate for the same gun and a list's rate hook always has its note.
		const auto rate = RE::ID::CombatFormulas::GetWeaponDisplayRateOfFire.address();

		g_cards = CallPatch::PatchTogether({
			{ RATE_SITES[0], rate, reinterpret_cast<std::uintptr_t>(&CardRateHk<0>), &g_cardLinks[0] },
			{ RATE_SITES[1], rate, reinterpret_cast<std::uintptr_t>(&ComparedRateHk<1>), &g_cardLinks[1] },
			{ RATE_SITES[2], rate, reinterpret_cast<std::uintptr_t>(&CardRateHk<2>), &g_cardLinks[2] },
		}, Part::kCardRate);
		if (g_cards) {
			REX::INFO("Item cards print the fire rate a worn gun fires at.");
		} else {
			REX::ERROR("Item cards will keep printing a worn gun at the fire rate of a new one.");
		}

		const auto types = RE::ID::PipboyInventoryUtils::FillDamageTypeInfo.address();
		g_better.held = CallPatch::PatchTogether({
			{ BETTER_TYPES_SITES[0], types, reinterpret_cast<std::uintptr_t>(&BetterTypesHk<0>), &g_betterTypeLinks[0] },
			{ BETTER_TYPES_SITES[1], types, reinterpret_cast<std::uintptr_t>(&BetterTypesHk<1>), &g_betterTypeLinks[1] },
			{ BETTER_RATE_SITES[0], rate, reinterpret_cast<std::uintptr_t>(&ListRateHk<0>), &g_listLinks[0] },
			{ BETTER_RATE_SITES[1], rate, reinterpret_cast<std::uintptr_t>(&ListRateHk<1>), &g_listLinks[1] },
		}, Part::kCardRate);
		if (g_better.held) {
			REX::INFO("The quick container weighs a worn gun at the fire rate it fires at before it marks an item better.");
		} else {
			REX::ERROR("The quick container will keep weighing a worn gun at the fire rate of a new one.");
		}

		g_sort.held = CallPatch::PatchTogether({
			{ SORT_MODS_SITE, RE::ID::BGSInventoryItem::GetInstanceData.address(), reinterpret_cast<std::uintptr_t>(&SortModsHk), &g_sortModsLink },
			{ SORT_RATE_SITE, rate, reinterpret_cast<std::uintptr_t>(&ListRateHk<2>), &g_listLinks[2] },
		}, Part::kCardRate);
		if (g_sort.held) {
			REX::INFO("Containers and traders sort a worn gun by the fire rate it fires at.");
		} else {
			REX::ERROR("Containers and traders will keep sorting a worn gun by the fire rate of a new one.");
		}

		return patched;
	}
}
