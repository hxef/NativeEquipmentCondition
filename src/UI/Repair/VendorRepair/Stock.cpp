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

		// How far this trader repairs each kind, kept until the stock is built
		// again. 0 is a trader outside that trade, and nothing at all is a
		// trader nobody has looked at yet.
		struct Ceilings
		{
			std::uint32_t weapons{ 0 };
			std::uint32_t armor{ 0 };
		};

		std::optional<Ceilings> g_ceilings;

		// The count last written to the trace log, which goes on only when it
		// changes, since the screen builds the trader's side several times.
		std::string g_logged;

		// Whether a_trade rows of one trade, with a_filler rows of what goes
		// with it, out of a_rows on the shelves, make this trader repair that
		// kind, and how far if so.
		[[nodiscard]] std::uint32_t CeilingOf(std::size_t a_trade, std::size_t a_filler, std::size_t a_rows)
		{
			const bool many = a_trade >= MANY_ROWS && a_trade * MANY_SHARE >= a_rows;
			const bool some = a_trade > 0 &&
			                  a_trade + a_filler >= SOME_ROWS &&
			                  (a_trade + a_filler) * SOME_SHARE >= a_rows;
			return many || some ? Reach(a_trade) : 0U;
		}

		// What the log calls what a trader repairs.
		[[nodiscard]] std::string Repairs(const Ceilings& a_ceilings)
		{
			std::string out;
			if (a_ceilings.weapons > 0) {
				out += std::format("weapons to {:d}%", a_ceilings.weapons);
			}
			if (a_ceilings.armor > 0) {
				out += std::format("{:s}armor to {:d}%", out.empty() ? "" : " and ", a_ceilings.armor);
			}
			return out.empty() ? std::string{ "nothing" } : out;
		}

		// What the shelves say about this trader. Everything is counted by row,
		// so a single crate of pistols cannot push a general store past the
		// threshold.
		[[nodiscard]] Ceilings ReadShelves(RE::BarterMenu* a_menu)
		{
			auto* inventory = RE::BGSInventoryInterface::GetSingleton();
			if (!a_menu || !inventory) {
				return {};
			}

			std::size_t rows = 0;
			std::size_t weapons = 0;
			std::size_t rounds = 0;
			std::size_t armor = 0;
			std::size_t powerArmor = 0;
			for (const auto& entry : a_menu->containerInv.stackedEntries) {
				const auto* item = inventory->RequestInventoryItem(entry.invHandle.id);
				if (!item || !item->object) {
					continue;
				}
				rows++;
				const auto& object = *item->object;
				if (Condition::WearsOut(object)) {
					if (Condition::KindOf(object) == Condition::Kind::kArmor) {
						armor++;
					} else {
						weapons++;
					}
				} else if (object.Is(RE::ENUM_FORM_ID::kAMMO)) {
					rounds++;
				} else if (object.Is(RE::ENUM_FORM_ID::kARMO)) {
					// The one wearable that does not wear, see ArmorWear.h.
					powerArmor++;
				}
			}

			const Ceilings out{ CeilingOf(weapons, rounds, rows), CeilingOf(armor, powerArmor, rows) };

			auto count = std::format("{:s} has {:d} rows of weapons, {:d} of ammunition, {:d} of armor and {:d} of power armor out of {:d}, and repairs {:s}",
				Trader(a_menu), weapons, rounds, armor, powerArmor, rows, Repairs(out));
			if (count != g_logged) {
				TraceLog::Line("menu", "{:s}", count);
				g_logged = std::move(count);
			}
			return out;
		}
	}

	std::uint32_t Reach(std::size_t a_rows)
	{
		if (a_rows == 0) {
			return 0;
		}
		if (a_rows >= FULL_ROWS) {
			return Repair::FULL;
		}

		// Worked in whole numbers, so no count lands just under a step it has
		// reached, then rounded down to the step at or below it.
		const auto climb = static_cast<std::uint32_t>(
			(Repair::FULL - MIN_CEILING) * (a_rows - 1) / (FULL_ROWS - 1));
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

	std::uint32_t Ceiling(RE::BarterMenu* a_menu, Condition::Kind a_kind)
	{
		if (!g_ceilings && a_menu && !a_menu->containerInv.stackedEntries.empty()) {
			g_ceilings = ReadShelves(a_menu);
		}
		if (!g_ceilings) {
			return 0;
		}
		return a_kind == Condition::Kind::kArmor ? g_ceilings->armor : g_ceilings->weapons;
	}

	void Forget()
	{
		g_row = -1;
		g_inContainer = false;
		g_ceilings.reset();
		g_logged.clear();
	}

	void ForgetShelves()
	{
		g_ceilings.reset();
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
