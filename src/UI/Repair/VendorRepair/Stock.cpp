#include "UI/Repair/VendorRepair/Stock.h"

#include "Condition/Condition.h"
#include "Core/TraceLog.h"
#include "Gameplay/ItemValue.h"

#include <format>
#include <optional>
#include <string>
#include <utility>

namespace VendorRepair
{
	namespace
	{
		// The levels are 10 apart. See Repair.h.
		constexpr std::uint32_t STEP = 10;

		// The row under the highlight and which of the 2 sides it is in.
		std::int32_t g_row = -1;
		bool         g_inContainer = false;

		// How far this trader repairs a weapon, kept until the stock is built
		// again. 0 is a trader outside the trade, and nothing is one nobody has
		// looked at yet.
		std::optional<std::uint32_t> g_ceiling;

		// The count last written to the trace log, which goes on only when it
		// changes, since the screen builds the trader's side several times.
		std::string g_logged;

		// What the shelves say about this trader. Everything is counted by row,
		// so a single crate of pistols cannot push a general store past the
		// threshold.
		[[nodiscard]] std::uint32_t ReadShelves(RE::BarterMenu* a_menu)
		{
			auto* inventory = RE::BGSInventoryInterface::GetSingleton();
			if (!a_menu || !inventory) {
				return 0;
			}

			std::size_t rows = 0;
			std::size_t guns = 0;
			std::size_t rounds = 0;
			for (const auto& entry : a_menu->containerInv.stackedEntries) {
				const auto* item = inventory->RequestInventoryItem(entry.invHandle.id);
				if (!item || !item->object) {
					continue;
				}
				rows++;
				if (Condition::WearsOut(*item->object)) {
					guns++;
				} else if (item->object->Is(RE::ENUM_FORM_ID::kAMMO)) {
					rounds++;
				}
			}

			const bool many = guns >= MANY_GUNS && guns * MANY_SHARE >= rows;
			const bool some = guns > 0 &&
			                  guns + rounds >= SOME_GUNS &&
			                  (guns + rounds) * SOME_SHARE >= rows;
			const auto ceiling = many || some ? Reach(guns) : 0U;

			auto count = std::format("{:s} has {:d} rows of weapons and {:d} of ammunition out of {:d}, and repairs {:s}",
				Trader(a_menu), guns, rounds, rows,
				ceiling > 0 ? std::format("weapons to {:d}%", ceiling) : "nothing");
			if (count != g_logged) {
				TraceLog::Line("menu", "{:s}", count);
				g_logged = std::move(count);
			}
			return ceiling;
		}
	}

	std::uint32_t Reach(std::size_t a_guns)
	{
		if (a_guns == 0) {
			return 0;
		}
		if (a_guns >= FULL_GUNS) {
			return Repair::FULL;
		}

		// Worked in whole numbers, so no count lands just under a step it has
		// reached, then rounded down to the step at or below it.
		const auto climb = static_cast<std::uint32_t>(
			(Repair::FULL - MIN_CEILING) * (a_guns - 1) / (FULL_GUNS - 1));
		return (MIN_CEILING + climb) / STEP * STEP;
	}

	void Highlight(std::int32_t a_row, bool a_inContainer)
	{
		g_row = a_row;
		g_inContainer = a_inContainer;
	}

	Selection Selected(RE::BarterMenu* a_menu)
	{
		if (!a_menu || g_inContainer || g_row < 0) {
			return {};
		}

		// The row number is a place in the sorted and filtered list, so the
		// menu turns it back into an entry the way its own price lookup does.
		const auto* entry = a_menu->GetInventoryItemByListIndex(false, static_cast<std::uint32_t>(g_row));
		if (!entry) {
			return {};
		}

		Selection out{ SelectedItem::Read(*entry) };
		if (!out.object) {
			return out;
		}

		// The sound price, with the wear and the trader's markup put aside.
		const auto worth = [&] {
			const ItemValue::ScopedSoundPrice sound;
			return out.item->GetInventoryValue(out.stack, false);
		}();
		out.worth = worth > 0 ? static_cast<std::uint32_t>(worth) : 0U;
		return out;
	}

	std::uint32_t Ceiling(RE::BarterMenu* a_menu)
	{
		if (!g_ceiling && a_menu && !a_menu->containerInv.stackedEntries.empty()) {
			g_ceiling = ReadShelves(a_menu);
		}
		return g_ceiling.value_or(0U);
	}

	void Forget()
	{
		g_row = -1;
		g_inContainer = false;
		g_ceiling.reset();
		g_logged.clear();
	}

	void ForgetShelves()
	{
		g_ceiling.reset();
	}

	bool Shown(const Selection& a_selection, std::uint32_t a_ceiling)
	{
		return a_selection.Worn() && a_ceiling > 0;
	}

	RE::BarterMenu* OpenBarter()
	{
		const auto* ui = RE::UI::GetSingleton();
		const auto  menu = ui ? ui->GetMenu<RE::BarterMenu>() : nullptr;
		return menu.get();
	}

	std::string Trader(RE::BarterMenu* a_menu)
	{
		if (a_menu) {
			const auto trader = a_menu->vendorActor.get();
			if (trader) {
				const auto* name = trader->GetDisplayFullName();
				if (name && *name) {
					return name;
				}
			}
		}
		return "The trader";
	}
}
