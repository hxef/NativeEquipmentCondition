#include "UI/Inventory/ItemCard/Cards.h"

#include "Condition/Condition.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/TraceLog.h"
#include "Gameplay/Aim/Aim.h"
#include "Gameplay/FireRate/FireRate.h"

#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace ItemCard
{
	namespace
	{
		// -------------------------------------------------------------------
		// Where the game asks for a fire rate
		// -------------------------------------------------------------------

		using CallPatch::CallSite;

		// The calls to CombatFormulas::GetWeaponDisplayRateOfFire inside the 2
		// card builders. The game hands them the weapon and its mods, not
		// which copy, so the hooks read that from t_building.
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

		// The second is the sort a container or a trader uses. By fire rate or
		// accuracy it reads each weapon's mods through GetInstanceData and
		// asks for that 1 figure right after. The Pip-Boy sorts by the figures
		// on its cards.
		constexpr CallSite SORT_MODS_SITE{ RE::ID::InventoryUserUIUtils::SortCompareItems.id(), 0x2EB, "sort mods" };
		constexpr CallSite SORT_RATE_SITE{ RE::ID::InventoryUserUIUtils::SortCompareItems.id(), 0x33E, "sort fire rate" };

		// The rate hooks of the cards, and the better check's note and rate
		// hooks. A list that weighs fire rates keeps its name for the trace.
		CallPatch::Held g_cards;
		CallPatch::Held g_betterHeld;
		struct List
		{
			const char* name;
		};

		List g_better{ "The better check" };
		List g_sort{ "The sort" };

		// Whether the sort's note went in, see SortNoted.
		bool g_sortNoted = false;

		// The condition of the stack a list weighs next, and the list.
		thread_local float       t_listHealth = Condition::INVALID_HEALTH;
		thread_local const List* t_list = nullptr;

		// The rate Link's type serves the card, compared and list rate sites
		// alike.
		using Rate_t = float (*)(const RE::TESObjectWEAP&, const RE::TESObjectWEAP::InstanceData*);
		using Types_t = void (*)(const RE::BGSInventoryItem&, const RE::BGSInventoryItem::Stack*, RE::BSScrapArray<RE::BSTTuple<std::uint32_t, float>>&);
		using Mods_t = RE::TBO_InstanceData* (*)(const RE::BGSInventoryItem&, std::uint32_t);

		std::array<CallPatch::Link<Rate_t>, std::size(RATE_SITES)>          g_cardLinks;
		std::array<CallPatch::Link<Types_t>, std::size(BETTER_TYPES_SITES)> g_betterTypeLinks;
		std::array<CallPatch::Link<Rate_t>, 3>                              g_listLinks;  // better rate x2, sort rate
		CallPatch::Link<Mods_t>                                             g_sortModsLink;

		// Takes the note a list wrote, so a call asked without its note never
		// takes the condition of the stack before it. Nothing when another
		// list, or none, wrote it.
		std::optional<float> TakeNote(const List& a_list)
		{
			const auto  health = std::exchange(t_listHealth, Condition::INVALID_HEALTH);
			const auto* list = std::exchange(t_list, nullptr);
			return list == &a_list ? std::optional{ health } : std::nullopt;
		}

		// The fire rate a card prints for a copy in this condition: the game's
		// own figure at the share the copy fires at, see FireRate.h.
		float RateAt(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data, float a_health,
			const CallPatch::Link<Rate_t>& a_link)
		{
			return a_link(a_weapon, a_data) * FireRate::RateShare(a_weapon, a_data, a_health);
		}

		// -------------------------------------------------------------------
		// The rate on the item cards
		// -------------------------------------------------------------------

		// Stands in for CombatFormulas::GetWeaponDisplayRateOfFire where a card
		// works out the fire rate of the item it is about. One per rate site.
		template <std::size_t I>
		float CardRateHk(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data)
		{
			if (!g_cards.Runs(g_cardLinks[I])) {
				return g_cardLinks[I](a_weapon, a_data);
			}
			return RateAt(a_weapon, a_data, ItemHealth(), g_cardLinks[I]);
		}

		// Stands in for it where the card works out the rate of the equipped
		// weapon the item is compared with.
		template <std::size_t I>
		float ComparedRateHk(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data)
		{
			if (!g_cards.Runs(g_cardLinks[I])) {
				return g_cardLinks[I](a_weapon, a_data);
			}
			return RateAt(a_weapon, a_data, ComparedHealth(&a_weapon), g_cardLinks[I]);
		}

		// -------------------------------------------------------------------
		// The rate in the quick container and the sort
		// -------------------------------------------------------------------

		// Stands in for PipboyInventoryUtils::FillDamageTypeInfo where the
		// better check adds up a weapon's damage types. One per site.
		template <std::size_t I>
		void BetterTypesHk(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack,
			RE::BSScrapArray<RE::BSTTuple<std::uint32_t, float>>& a_damageValuesPerType)
		{
			if (g_betterHeld.Runs(g_betterTypeLinks[I])) {
				t_listHealth = Condition::HealthOf(a_stack);
				t_list = &g_better;
			}
			g_betterTypeLinks[I](a_item, a_stack, a_damageValuesPerType);
		}

		// Stands in for BGSInventoryItem::GetInstanceData where the sort reads
		// a weapon's mods.
		RE::TBO_InstanceData* SortModsHk(const RE::BGSInventoryItem& a_item, std::uint32_t a_stackID)
		{
			if (g_sortModsLink.Live()) {
				t_listHealth = Condition::HealthOf(a_item.GetStackByID(a_stackID));
				t_list = &g_sort;
			}
			return g_sortModsLink(a_item, a_stackID);
		}

		// Stands in for CombatFormulas::GetWeaponDisplayRateOfFire in both
		// lists, I being 0 or 1 at the better check and 2 at the sort. It asks
		// on every call whether its place runs, and scales only what its own
		// list noted. The sort's rate place stands alone, and a note from the
		// sort shows the note's own place ran.
		template <std::size_t I>
		float ListRateHk(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data)
		{
			const auto& own = I < 2 ? g_better : g_sort;
			const auto  note = TakeNote(own);
			const auto  runs = I < 2 ? g_betterHeld.Runs(g_listLinks[I]) : g_listLinks[I].Live();
			if (!runs || !note) {
				return g_listLinks[I](a_weapon, a_data);
			}
			const auto health = *note;
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

		// -------------------------------------------------------------------
		// Patching
		// -------------------------------------------------------------------

		// The cards' rate set and the better check's. Each set goes in whole
		// or not at all, so every card prints the same rate for the same gun
		// and a list's rate hook always has its note.
		void PatchCardRates(std::uintptr_t a_rate)
		{
			g_cards = CallPatch::PatchTogether({
				{ RATE_SITES[0], a_rate, reinterpret_cast<std::uintptr_t>(&CardRateHk<0>), &g_cardLinks[0] },
				{ RATE_SITES[1], a_rate, reinterpret_cast<std::uintptr_t>(&ComparedRateHk<1>), &g_cardLinks[1] },
				{ RATE_SITES[2], a_rate, reinterpret_cast<std::uintptr_t>(&CardRateHk<2>), &g_cardLinks[2] },
			}, Part::kCardRate);
			if (g_cards) {
				REX::INFO("Item cards print the fire rate a worn gun fires at.");
			} else {
				REX::WARN("Item cards will keep printing a worn gun at the fire rate of a new one.");
			}

			const auto types = RE::ID::PipboyInventoryUtils::FillDamageTypeInfo.address();
			g_betterHeld = CallPatch::PatchTogether({
				{ BETTER_TYPES_SITES[0], types, reinterpret_cast<std::uintptr_t>(&BetterTypesHk<0>), &g_betterTypeLinks[0] },
				{ BETTER_TYPES_SITES[1], types, reinterpret_cast<std::uintptr_t>(&BetterTypesHk<1>), &g_betterTypeLinks[1] },
				{ BETTER_RATE_SITES[0], a_rate, reinterpret_cast<std::uintptr_t>(&ListRateHk<0>), &g_listLinks[0] },
				{ BETTER_RATE_SITES[1], a_rate, reinterpret_cast<std::uintptr_t>(&ListRateHk<1>), &g_listLinks[1] },
			}, Part::kCardRate);
			if (g_betterHeld) {
				REX::INFO("The quick container weighs a worn gun at the fire rate it fires at before it marks an item better.");
			} else {
				REX::WARN("The quick container will keep weighing a worn gun at the fire rate of a new one.");
			}
		}
	}

	bool SortNoted()
	{
		return g_sortNoted;
	}

	std::optional<float> TakeSortNote()
	{
		return TakeNote(g_sort);
	}

	float ItemHealth()
	{
		return t_building ? t_building->health : Condition::INVALID_HEALTH;
	}

	float ComparedHealth(const RE::TESForm* a_weapon)
	{
		if (!t_building || !t_building->compare || !a_weapon) {
			return Condition::INVALID_HEALTH;
		}
		for (const auto& equipped : *t_building->compare) {
			if (equipped.first && equipped.first->object == a_weapon) {
				return Condition::HealthOf(equipped.first->GetStackByID(equipped.second));
			}
		}
		return Condition::INVALID_HEALTH;
	}

	void PatchRates()
	{
		// While a worn gun fires as fast as a new one, the game's own fire rate
		// is the true one, so the menus keep it.
		const auto slows = FireRate::Slows();
		const auto rate = RE::ID::CombatFormulas::GetWeaponDisplayRateOfFire.address();
		if (slows) {
			PatchCardRates(rate);
		}

		// The sort's note serves its rate and accuracy, so it goes in while
		// either does, as a place of its own, under the part Inventory
		// sorting. Both sort places go alone too, so another DLL on one of
		// them leaves the note and the other working.
		if (!slows && !Aim::Spreads()) {
			return;
		}
		g_sortNoted = CallPatch::PatchCall(SORT_MODS_SITE, RE::ID::BGSInventoryItem::GetInstanceData.address(),
			reinterpret_cast<std::uintptr_t>(&SortModsHk), g_sortModsLink, Part::kSort);
		if (!slows) {
			return;
		}
		const auto sorts = CallPatch::PatchCall(SORT_RATE_SITE, rate, reinterpret_cast<std::uintptr_t>(&ListRateHk<2>), g_listLinks[2],
			Part::kCardRate);
		if (sorts && SortNoted()) {
			REX::INFO("Containers and traders sort a worn gun by the fire rate it fires at.");
		} else {
			REX::WARN("Containers and traders will keep sorting a worn gun by the fire rate of a new one.");
		}
	}
}
