#include "Gameplay/Scrapping/Scrapping.h"

#include "Condition/Condition.h"
#include "Condition/Repair.h"
#include "Core/CallPatch/CallPatch.h"

#include <cstddef>

namespace Scrapping
{
	namespace
	{
		// The 2 calls of Workshop::BuildScrapRewards that make a scrap list:
		// RequestScrap's for the $SCRAP box, and ScrapReference's when it is
		// handed no list, for what is linked to a scrapped object and for the
		// console's cell sweep. The box's yes hands its own list on, so it
		// never builds a 2nd one.
		constexpr CallPatch::CallSite BOX_SITE{ CallPatch::AE{ 2195194, 0xDB }, CallPatch::OG{ 1402505, 0x61 }, "workshop scrap box" };
		constexpr CallPatch::CallSite NO_BOX_SITE{ CallPatch::AE{ 2195125, 0x386 }, CallPatch::OG{ 636327, 0x2CB }, "workshop scrap no box" };

		// The tail jump of Workshop::ScrapRewardCount into ScrapRewardCount,
		// once per component of a list. The workshop's info card runs it too,
		// with no list built, and stays vanilla.
		constexpr CallPatch::CallSite COUNT_SITE{ CallPatch::AE{ 2194969, 0xC5, true }, CallPatch::OG{ 1531126, 0xC5, true }, "workshop scrap count" };

		using ListLink = CallPatch::Link<decltype(&RE::Workshop::BuildScrapRewards)>;

		// The count only scales the list the list calls noted an item for,
		// so the 3 go in together.
		CallPatch::Held g_lists;
		ListLink        g_listLinks[2];
		CountLink       g_countLink;

		// The item a list is being built for. A list is built on 1 thread,
		// from the list call to its counts, so a thread local carries it.
		// Empty for an item that scraps as vanilla.
		struct Noted
		{
			const RE::TESBoundObject* item{ nullptr };
			std::uint32_t             percent{ 0 };
		};

		thread_local Noted t_noted{};

		Noted NoteOf(RE::TESObjectREFR* a_ref)
		{
			const auto* item = a_ref ? a_ref->GetObjectReference() : nullptr;
			const auto  percent = item ? Condition::Percent(*item, a_ref->extraList.get()) : std::nullopt;
			if (!percent || *percent >= Repair::FULL) {
				return {};
			}
			return Noted{ item, static_cast<std::uint32_t>(*percent) };
		}

		template <std::size_t I>
		void ListHk(RE::TESObjectREFR* a_ref, bool a_ownRecipeOnly, RE::BSScrapArray<RE::BSTTuple<RE::TESBoundObject*, std::uint32_t>>** a_rewards)
		{
			// Scrapping 1 object can scrap what is linked to it, a list built
			// inside another, so the outer item is put back after.
			const auto outer = t_noted;
			Tally      tally;
			t_noted = g_lists.Runs(g_listLinks[I]) ? NoteOf(a_ref) : Noted{};
			g_listLinks[I](a_ref, a_ownRecipeOnly, a_rewards);
			t_noted = outer;

			// 1 component, not 1 for each copy a dropped stack holds.
			auto* part = tally.LeastPart("workshop");
			if (part && a_rewards && *a_rewards) {
				(*a_rewards)->emplace_back(part, 1U);
			}
		}

		std::uint32_t CountHk(RE::TESForm* a_form, std::uint32_t a_count, bool a_applyBase, bool a_skipScalar)
		{
			const auto noted = t_noted;
			if (!g_lists.Runs(g_countLink) || !noted.item) {
				return g_countLink(a_form, a_count, a_applyBase, a_skipScalar);
			}
			return Count(g_countLink, "workshop", *noted.item, noted.percent, a_form, a_count, a_applyBase, a_skipScalar);
		}
	}

	void InstallWorkshop()
	{
		g_lists = CallPatch::PatchTogether({
			{ BOX_SITE, RE::ID::Workshop::BuildScrapRewards, reinterpret_cast<std::uintptr_t>(&ListHk<0>), &g_listLinks[0] },
			{ NO_BOX_SITE, RE::ID::Workshop::BuildScrapRewards, reinterpret_cast<std::uintptr_t>(&ListHk<1>), &g_listLinks[1] },
			{ COUNT_SITE, RE::ID::ScrapRewardCount, reinterpret_cast<std::uintptr_t>(&CountHk), &g_countLink },
		});

		if (g_lists) {
			REX::INFO("Worn weapons and armor scrap into fewer components in workshop mode.");
		} else {
			REX::WARN("Worn weapons and armor will scrap into as many components as new ones in workshop mode.");
		}
	}
}
