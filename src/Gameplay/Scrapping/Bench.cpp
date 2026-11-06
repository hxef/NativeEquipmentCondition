#include "Gameplay/Scrapping/Scrapping.h"

#include "Condition/Condition.h"
#include "Condition/Repair.h"
#include "Core/BenchRow.h"
#include "Core/CallPatch/CallPatch.h"

#include <optional>

namespace Scrapping
{
	namespace
	{
		// The count call inside ExamineMenu::BuildWeaponScrappingArray, once
		// per component its recipes add up. Its only caller is the bench's
		// SCRAP button, before the "Scrap item?" box.
		constexpr CallPatch::CallSite COUNT_SITE{ CallPatch::AE{ 2223077, 0x36C }, CallPatch::OG{ 646841, 0x27C }, "bench scrap count" };

		// That only caller, the SCRAP button's case of ExamineMenu::Call. The
		// list is whole once the call returns, and the box and the yes both
		// read it from the menu after.
		constexpr CallPatch::CallSite LIST_SITE{ CallPatch::AE{ 2223002, 0xBE8 }, CallPatch::OG{ 1461108, 0xC39 }, "bench scrap list" };

		using ListLink = CallPatch::Link<void(RE::ExamineMenu*)>;

		// The list hook reads what the count hook gave, so the 2 go in
		// together.
		CallPatch::Held g_bench;
		ListLink        g_listLink;
		CountLink       g_countLink;

		// The copy the SCRAP yes takes, as its whole percent, or nothing for
		// a row that does not wear. A row can hold several stacks. The yes
		// takes the first one that is not equipped, else the row's first, so
		// a chest piece the player wears never scraps at full.
		struct Copy
		{
			const RE::TESBoundObject* item{ nullptr };
			std::uint32_t             percent{ 0 };
		};

		std::optional<Copy> Scrapped()
		{
			const auto* ui = RE::UI::GetSingleton();
			const auto  open = ui ? ui->GetMenu<RE::ExamineMenu>() : nullptr;
			const auto* entry = open ? BenchRow::Highlighted(*open) : nullptr;
			auto*       inventory = RE::BGSInventoryInterface::GetSingleton();
			const auto* item = entry && inventory ? inventory->RequestInventoryItem(entry->invHandle.id) : nullptr;
			if (!item || !item->object || entry->stackIndex.size() == 0) {
				return std::nullopt;
			}

			const RE::BGSInventoryItem::Stack* taken = item->GetStackByID(entry->stackIndex[0]);
			for (const auto id : entry->stackIndex) {
				const auto* stack = item->GetStackByID(id);
				if (stack && !stack->IsEquipped()) {
					taken = stack;
					break;
				}
			}
			const auto percent = Condition::Percent(*item, taken);
			if (!percent) {
				return std::nullopt;
			}
			return Copy{ item->object, static_cast<std::uint32_t>(*percent) };
		}

		std::uint32_t CountHk(RE::TESForm* a_form, std::uint32_t a_count, bool a_applyBase, bool a_skipScalar)
		{
			const auto copy = g_bench.Runs(g_countLink) ? Scrapped() : std::nullopt;
			if (!copy || copy->percent >= Repair::FULL) {
				return g_countLink(a_form, a_count, a_applyBase, a_skipScalar);
			}
			return Count(g_countLink, "bench", *copy->item, copy->percent, a_form, a_count, a_applyBase, a_skipScalar);
		}

		void ListHk(RE::ExamineMenu* a_menu)
		{
			if (!g_bench.Runs(g_listLink)) {
				g_listLink(a_menu);
				return;
			}
			Tally tally;
			g_listLink(a_menu);
			// The bench scraps 1 item at a time.
			if (auto* part = tally.LeastPart("bench", 1)) {
				a_menu->scrappingArray.emplace_back(part, 1U);
			}
		}
	}

	void InstallBench()
	{
		g_bench = CallPatch::PatchTogether({
			{ LIST_SITE, RE::ID::ExamineMenu::BuildWeaponScrappingArray, reinterpret_cast<std::uintptr_t>(&ListHk), &g_listLink },
			{ COUNT_SITE, RE::ID::ScrapRewardCount, reinterpret_cast<std::uintptr_t>(&CountHk), &g_countLink },
		});

		if (g_bench) {
			REX::INFO("Worn weapons and armor scrap into fewer components at a workbench.");
		} else {
			REX::WARN("Worn weapons and armor will scrap into as many components as new ones at a workbench.");
		}
	}
}
