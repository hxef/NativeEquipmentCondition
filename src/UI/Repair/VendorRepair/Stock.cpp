#include "UI/Repair/VendorRepair/Stock.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/Condition.h"
#include "Core/TraceLog.h"
#include "Gameplay/ItemValue.h"
#include "UI/Repair/VendorRepair/Restock.h"

#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <vector>

namespace VendorRepair
{
	namespace
	{
		// The levels are 10 apart. See Repair.h.
		constexpr std::uint32_t STEP = 10;

		// The row under the highlight and which of the 2 sides it is in.
		std::int32_t g_row = -1;
		bool         g_inContainer = false;

		// How far this trader repairs each kind, kept until the trader's side
		// is built again. Empty until a worn item asks again.
		std::optional<Ceilings> g_ceilings;

		// Whether a_count rows of a_trade, with a_filler rows of what goes with
		// it, out of a_rows in stock, make this trader repair that kind, and
		// how far if so.
		[[nodiscard]] std::uint32_t CeilingOf(Trade a_trade, std::size_t a_count, std::size_t a_filler, std::size_t a_rows)
		{
			const bool many = a_count >= MANY_ROWS && a_count * MANY_SHARE >= a_rows;
			const bool some = a_count > 0 &&
			                  a_count + a_filler >= SOME_ROWS &&
			                  (a_count + a_filler) * SOME_SHARE >= a_rows;
			return many || some ? Reach(a_trade, a_count) : 0U;
		}

		// A count of rows rounded to whole, half up. The chances are added in
		// no set order, which can leave a sum of exactly a half just short of
		// it, and that still counts as the half.
		[[nodiscard]] std::size_t Whole(double a_rows)
		{
			return static_cast<std::size_t>(std::floor(a_rows + 0.5 + 1e-9));
		}
	}

	std::uint32_t Ceilings::Of(Trade a_trade) const
	{
		switch (a_trade) {
		case Trade::kArmor:
			return armor;
		case Trade::kClothing:
			return clothing;
		default:
			return weapons;
		}
	}

	std::string Repairs(const Ceilings& a_ceilings)
	{
		std::vector<std::string> each;
		for (const auto trade : { Trade::kWeapons, Trade::kArmor, Trade::kClothing }) {
			const auto ceiling = a_ceilings.Of(trade);
			if (ceiling > 0) {
				each.push_back(std::format("{:s} to {:d}%", Named(trade), ceiling));
			}
		}
		if (each.empty()) {
			return "nothing";
		}

		std::string out;
		for (std::size_t i = 0; i < each.size(); i++) {
			if (i > 0) {
				out += i + 1 < each.size() ? ", " : " and ";
			}
			out += each[i];
		}
		return out;
	}

	Trade TradeOf(const RE::TESBoundObject& a_object)
	{
		const auto* armor = a_object.As<RE::TESObjectARMO>();
		if (!armor) {
			return Trade::kWeapons;
		}
		return ArmorWear::IsClothing(*armor) ? Trade::kClothing : Trade::kArmor;
	}

	// A row is a different item, counted by its chance of turning up, so a
	// crate of pistols is 1 row and cannot push a general store past the
	// threshold.
	Ceilings CeilingsOf(RE::TESObjectREFR* a_merchant, RE::Actor& a_trader, std::string_view a_now)
	{
		double rows = 0.0;
		double weapons = 0.0;
		double rounds = 0.0;
		double thrown = 0.0;
		double armor = 0.0;
		double powerArmor = 0.0;
		double clothing = 0.0;
		for (const auto& [object, chance] : Restock(a_merchant, a_trader)) {
			rows += chance;
			if (Condition::WearsOut(*object)) {
				switch (TradeOf(*object)) {
				case Trade::kWeapons:
					weapons += chance;
					break;
				case Trade::kArmor:
					armor += chance;
					break;
				case Trade::kClothing:
					clothing += chance;
					break;
				}
			} else if (object->Is(RE::ENUM_FORM_ID::kAMMO)) {
				rounds += chance;
			} else if (const auto* weapon = object->As<RE::TESObjectWEAP>(); weapon && weapon->IsThrownWeapon()) {
				thrown += chance;
			} else if (const auto* piece = object->As<RE::TESObjectARMO>(); piece && ArmorWear::IsPowerArmor(*piece)) {
				// Armor's filler, see Stock.h.
				powerArmor += chance;
			}
		}

		// Each on its own rows, and nothing stands beside clothing, see
		// Stock.h.
		const Ceilings out{
			CeilingOf(Trade::kWeapons, Whole(weapons), Whole(rounds) + Whole(thrown), Whole(rows)),
			CeilingOf(Trade::kArmor, Whole(armor), Whole(powerArmor), Whole(rows)),
			CeilingOf(Trade::kClothing, Whole(clothing), 0, Whole(rows)),
		};
		TraceLog::Line("menu", "{:s} restocks {:d} rows of weapons, {:d} of ammunition, {:d} of grenades and mines, {:d} of armor, {:d} of power armor and {:d} of clothing out of {:d}{:s}, and repairs {:s}",
			Trader(&a_trader), Whole(weapons), Whole(rounds), Whole(thrown), Whole(armor), Whole(powerArmor), Whole(clothing),
			Whole(rows), a_now, Repairs(out));
		return out;
	}

	std::uint32_t Reach(Trade a_trade, std::size_t a_rows)
	{
		const auto full = FullRows(a_trade);
		if (a_rows == 0) {
			return 0;
		}
		if (a_rows >= full) {
			return Repair::FULL;
		}

		// Worked in whole numbers, so no count lands just under a step it has
		// reached, then rounded down to the step at or below it.
		const auto climb = static_cast<std::uint32_t>(
			(Repair::FULL - MIN_CEILING) * (a_rows - 1) / (full - 1));
		return (MIN_CEILING + climb) / STEP * STEP;
	}

	std::string_view Named(Trade a_trade)
	{
		switch (a_trade) {
		case Trade::kArmor:
			return "armor"sv;
		case Trade::kClothing:
			return "clothing"sv;
		default:
			return "weapons"sv;
		}
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

		// The rows before the player's own are what they are buying, still
		// the trader's until the trade goes through.
		const auto row = static_cast<std::uint32_t>(g_row);
		if (row < a_menu->playerTentativeInv.stackedEntries.size()) {
			return {};
		}

		// The row number is a place in the sorted and filtered list, so the
		// menu turns it back into an entry the way its own price lookup does.
		const auto* entry = a_menu->GetInventoryItemByListIndex(false, row);
		if (!entry) {
			return {};
		}

		Selection out{ SelectedItem::Read(*entry) };
		if (!out.object) {
			return out;
		}
		out.trade = TradeOf(*out.object);

		// The sound price, with the wear and the trader's markup put aside.
		const auto worth = [&] {
			const ItemValue::ScopedSoundPrice sound;
			return out.item->GetInventoryValue(out.stack, false);
		}();
		out.worth = worth > 0 ? static_cast<std::uint32_t>(worth) : 0U;
		return out;
	}

	std::uint32_t Ceiling(RE::BarterMenu* a_menu, const Selection& a_selection)
	{
		if (!a_menu || !a_selection.Worn()) {
			return 0;
		}
		if (!g_ceilings) {
			// What the open screen's trader restocks with says about them.
			const auto handle = a_menu->vendorActor.get();
			auto*      trader = handle ? handle->As<RE::Actor>() : nullptr;
			if (!trader) {
				return 0;
			}
			g_ceilings = CeilingsOf(a_menu->vendorChestRef.get().get(), *trader,
				std::format(", shows {:d} rows now", a_menu->containerInv.stackedEntries.size()));
		}
		return g_ceilings->Of(a_selection.trade);
	}

	void Forget()
	{
		g_row = -1;
		g_inContainer = false;
		ForgetStock();
	}

	void ForgetStock()
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
		const auto trader = a_menu ? a_menu->vendorActor.get() : RE::NiPointer<RE::TESObjectREFR>{};
		return Trader(trader.get());
	}

	std::string Trader(RE::TESObjectREFR* a_trader)
	{
		const auto* name = a_trader ? a_trader->GetDisplayFullName() : nullptr;
		return name && *name ? name : "The trader";
	}
}
