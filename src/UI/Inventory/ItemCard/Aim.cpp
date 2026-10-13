#include "UI/Inventory/ItemCard/Cards.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/TraceLog.h"
#include "Gameplay/Aim/Aim.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ItemCard
{
	namespace
	{
		// -------------------------------------------------------------------
		// Where the game asks for accuracy and range
		// -------------------------------------------------------------------

		using CallPatch::CallSite;
		using Weapon = RE::BGSObjectInstanceT<RE::TESObjectWEAP>;

		// The calls to CombatFormulas::GetWeaponDisplayAccuracy and
		// GetWeaponDisplayRange inside the 2 card builders, beside the fire
		// rate calls: the item on the card and the equipped weapon it is
		// compared with, then the Pip-Boy's card. The Pip-Boy's compare arrows
		// and its sort read the figures on its cards, so they follow.
		constexpr CallSite ACCURACY_SITES[]{
			{ RE::ID::InventoryUserUIUtils::PopulateItemCardInfo_Helper.id(), 0x917, "card accuracy" },
			{ RE::ID::InventoryUserUIUtils::PopulateItemCardInfo_Helper.id(), 0x935, "card accuracy compared" },
			{ RE::ID::PipboyInventoryData::PopulateItemCardInfo.id(), 0x497, "pipboy card accuracy" },
		};
		constexpr CallSite RANGE_SITES[]{
			{ RE::ID::InventoryUserUIUtils::PopulateItemCardInfo_Helper.id(), 0x87C, "card range" },
			{ RE::ID::InventoryUserUIUtils::PopulateItemCardInfo_Helper.id(), 0x898, "card range compared" },
			{ RE::ID::PipboyInventoryData::PopulateItemCardInfo.id(), 0x472, "pipboy card range" },
		};

		// The sort a container or a trader uses, by accuracy, right after its
		// note of the stack, see TakeSortNote. The game never sorts those
		// lists by range, so NEC leaves the sort's range call alone.
		constexpr CallSite SORT_ACCURACY_SITE{ RE::ID::InventoryUserUIUtils::SortCompareItems.id(), 0x368, "sort accuracy" };

		using Accuracy_t = float (*)(const Weapon&, RE::Actor*);
		using Range_t = float (*)(const Weapon&);

		CallPatch::Held                                                    g_accuracyCards;
		CallPatch::Held                                                    g_rangeCards;
		std::array<CallPatch::Link<Accuracy_t>, std::size(ACCURACY_SITES)> g_accuracyLinks;
		std::array<CallPatch::Link<Range_t>, std::size(RANGE_SITES)>       g_rangeLinks;
		CallPatch::Link<Accuracy_t>                                        g_sortAccuracyLink;

		// The weapon on the card, or null for anything else. The shares give 1
		// for anything but a gun.
		const RE::TESObjectWEAP* WeaponOf(const Weapon& a_weapon)
		{
			const auto* object = a_weapon.object;
			return object && object->IsWeapon() ? static_cast<const RE::TESObjectWEAP*>(object) : nullptr;
		}

		// The condition a card hook reads: the card's own item for I 0 and
		// 2, the equipped weapon it is compared with for I 1.
		template <std::size_t I>
		float CardHealth(const Weapon& a_weapon)
		{
			if constexpr (I == 1) {
				return ComparedHealth(a_weapon.object);
			} else {
				return ItemHealth();
			}
		}

		// -------------------------------------------------------------------
		// The cards
		// -------------------------------------------------------------------

		// Stands in for CombatFormulas::GetWeaponDisplayAccuracy at the card
		// sites. The card prints the game's own figure at the share the copy
		// keeps, see Aim.h.
		template <std::size_t I>
		float CardAccuracyHk(const Weapon& a_weapon, RE::Actor* a_actor)
		{
			const auto  accuracy = g_accuracyLinks[I](a_weapon, a_actor);
			const auto* weapon = WeaponOf(a_weapon);
			if (!g_accuracyCards.Runs(g_accuracyLinks[I]) || !weapon) {
				return accuracy;
			}
			return accuracy * Aim::AccuracyShare(*weapon, CardHealth<I>(a_weapon));
		}

		// Stands in for CombatFormulas::GetWeaponDisplayRange at the card
		// sites, the same way.
		template <std::size_t I>
		float CardRangeHk(const Weapon& a_weapon)
		{
			const auto  range = g_rangeLinks[I](a_weapon);
			const auto* weapon = WeaponOf(a_weapon);
			if (!g_rangeCards.Runs(g_rangeLinks[I]) || !weapon) {
				return range;
			}
			return range * Aim::RangeShare(*weapon, CardHealth<I>(a_weapon));
		}

		// -------------------------------------------------------------------
		// The sort
		// -------------------------------------------------------------------

		// Stands in for GetWeaponDisplayAccuracy in the sort. The note is
		// taken first, so it never waits for the next stack.
		float SortAccuracyHk(const Weapon& a_weapon, RE::Actor* a_actor)
		{
			const auto  note = TakeSortNote();
			const auto  accuracy = g_sortAccuracyLink(a_weapon, a_actor);
			const auto* weapon = WeaponOf(a_weapon);
			if (!note || !g_sortAccuracyLink.Live() || !weapon) {
				return accuracy;
			}
			// The trace names a worn gun the sort weighs below its own
			// figure, once per gun as ListRateHk in Rate.cpp does.
			const auto share = Aim::AccuracyShare(*weapon, *note);
			if (share < 1.0F) {
				TraceLog::Once("menu", "The sort weighs {:s} [{:08X}] at condition {:.3f} at a card accuracy of {:.2f}, {:.3f} of its own",
					RE::TESFullName::GetFullName(*weapon), weapon->formID, *note, accuracy * share, share);
			}
			return accuracy * share;
		}
	}

	void PatchAims()
	{
		// While a worn gun keeps its accuracy, or its range, the game's own
		// figure is the true one, so the menus keep it. Each card set goes in
		// whole, so every card prints the same figure for the same gun. The
		// accuracy sort place goes alone and reads the sort's note, so it
		// sorts only while that note goes in too.
		if (Aim::Spreads()) {
			const auto accuracy = RE::ID::CombatFormulas::GetWeaponDisplayAccuracy.address();
			g_accuracyCards = CallPatch::PatchTogether({
				{ ACCURACY_SITES[0], accuracy, reinterpret_cast<std::uintptr_t>(&CardAccuracyHk<0>), &g_accuracyLinks[0] },
				{ ACCURACY_SITES[1], accuracy, reinterpret_cast<std::uintptr_t>(&CardAccuracyHk<1>), &g_accuracyLinks[1] },
				{ ACCURACY_SITES[2], accuracy, reinterpret_cast<std::uintptr_t>(&CardAccuracyHk<2>), &g_accuracyLinks[2] },
			}, Part::kCardAccuracy);
			if (g_accuracyCards) {
				REX::INFO("Item cards print the accuracy a worn gun keeps.");
			} else {
				REX::WARN("Item cards will keep printing a worn gun at the accuracy of a new one.");
			}
			const auto sorts = CallPatch::PatchCall(SORT_ACCURACY_SITE, accuracy, reinterpret_cast<std::uintptr_t>(&SortAccuracyHk), g_sortAccuracyLink,
				Part::kCardAccuracy);
			if (sorts && SortNoted()) {
				REX::INFO("Containers and traders sort a worn gun by the accuracy it keeps.");
			} else {
				REX::WARN("Containers and traders will keep sorting a worn gun by the accuracy of a new one.");
			}
		}

		if (Aim::Shortens()) {
			const auto range = RE::ID::CombatFormulas::GetWeaponDisplayRange.address();
			g_rangeCards = CallPatch::PatchTogether({
				{ RANGE_SITES[0], range, reinterpret_cast<std::uintptr_t>(&CardRangeHk<0>), &g_rangeLinks[0] },
				{ RANGE_SITES[1], range, reinterpret_cast<std::uintptr_t>(&CardRangeHk<1>), &g_rangeLinks[1] },
				{ RANGE_SITES[2], range, reinterpret_cast<std::uintptr_t>(&CardRangeHk<2>), &g_rangeLinks[2] },
			}, Part::kCardRange);
			if (g_rangeCards) {
				REX::INFO("Item cards print the range a worn gun keeps.");
			} else {
				REX::WARN("Item cards will keep printing a worn gun at the range of a new one.");
			}
		}
	}
}
