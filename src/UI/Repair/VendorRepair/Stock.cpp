#include "UI/Repair/VendorRepair/Stock.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/Condition.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/ItemValue.h"
#include "UI/Repair/VendorRepair/Restock.h"
#include "UI/Repair/VendorRepair/TraderList.h"
#include "UI/Repair/VendorRepair/VendorRepair.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace VendorRepair
{
	namespace
	{
		// The levels are 10 apart. See Repair.h.
		constexpr std::uint32_t STEP = 10;

		// The 3 kinds in the order the trace log notes them.
		constexpr Trade TRADES[]{ Trade::kWeapons, Trade::kArmor, Trade::kClothing };

		// The row under the highlight and which of the 2 sides it is in.
		std::int32_t g_row = -1;
		bool         g_inContainer = false;

		// How far this trader repairs each kind, kept until the trader's side
		// is built again. Empty until a worn item asks again.
		std::optional<Ceilings> g_ceilings;

		// How far a trader repairs a kind, and for the trace log how far past
		// its share of the stock other goods took it, in percent and in steps
		// of 10% lower.
		struct Measured
		{
			std::uint32_t ceiling{ 0 };
			std::size_t   past{ 0 };
			std::size_t   steps{ 0 };
		};

		// How far a_count rows of a_trade, with a_filler rows of what goes
		// with it, out of a_rows in stock, make this trader repair that kind.
		[[nodiscard]] Measured CeilingOf(Trade a_trade, std::size_t a_count, std::size_t a_filler, std::size_t a_rows)
		{
			// The most rows either test lets the stock hold, 0 for a kind too
			// thin for both.
			std::size_t allowed = a_count >= MANY_ROWS ? a_count * MANY_SHARE : 0;
			if (a_count > 0 && a_count + a_filler >= SOME_ROWS) {
				allowed = std::max(allowed, (a_count + a_filler) * SOME_SHARE);
			}
			if (allowed == 0) {
				return {};
			}
			const auto top = Reach(a_trade, a_count);
			if (a_rows <= allowed) {
				return { top };
			}

			// Rounded up on purpose, so 1 row past already costs a step, and
			// the percent the trace shows gives the same steps.
			const auto over = a_rows - allowed;
			const auto past = (over * 100 + allowed - 1) / allowed;
			const auto leeway = Leeway();
			if (leeway == 0) {
				return { 0, past, 0 };
			}
			const auto steps = (over * 100 + leeway * allowed - 1) / (leeway * allowed);
			const auto lower = static_cast<std::uint32_t>(steps) * STEP;
			return { top >= MIN_CEILING + lower ? top - lower : 0U, past, steps };
		}

		// For the trace log, how far other goods took a_trade down, as ",
		// armor 2 steps lower at 12% past its share".
		[[nodiscard]] std::string Lowered(Trade a_trade, const Measured& a_measured)
		{
			if (a_measured.past == 0) {
				return {};
			}
			if (a_measured.ceiling == 0) {
				return std::format(", {:s} none at {:d}% past its share", Named(a_trade), a_measured.past);
			}
			return std::format(", {:s} {:d} {:s} lower at {:d}% past its share", Named(a_trade), a_measured.steps,
				a_measured.steps == 1 ? "step" : "steps", a_measured.past);
		}

		// A count of rows rounded to whole, half up. The chances are added in
		// no set order, which can leave a sum of exactly a half just short of
		// it, and that still counts as the half.
		[[nodiscard]] std::size_t Whole(double a_rows)
		{
			return static_cast<std::size_t>(std::floor(a_rows + 0.5 + 1e-9));
		}

		// "100%", or "none" for a level of 0.
		[[nodiscard]] std::string LevelText(std::uint32_t a_level)
		{
			return a_level > 0 ? std::format("{:d}%", a_level) : "none";
		}

		// The levels the trader list sets in place of a_ceilings, see
		// TraderList.h, and for the trace log, kind by kind, how far other
		// goods took it down, see Lowered, and what each line set, as ",
		// armor 90% by Traders/MyTraders.ini line 4, measured 70%".
		[[nodiscard]] std::string Listed(const RE::Actor& a_trader, const Measured (&a_measured)[std::size(TRADES)], Ceilings& a_ceilings)
		{
			std::string notes;
			for (std::size_t i = 0; i < std::size(TRADES); i++) {
				const auto trade = TRADES[i];
				notes += Lowered(trade, a_measured[i]);
				const auto set = TraderList::Find(a_trader, trade);
				if (!set) {
					continue;
				}
				auto& level = trade == Trade::kWeapons ? a_ceilings.weapons : trade == Trade::kArmor ? a_ceilings.armor : a_ceilings.clothing;
				if (!set->level) {
					notes += std::format(", {:s} measured by {:s} line {:d}", Named(trade), set->file, set->line);
					continue;
				}
				notes += std::format(", {:s} {:s} by {:s} line {:d}, measured {:s}", Named(trade), LevelText(*set->level), set->file, set->line,
					LevelText(level));
				level = *set->level;
			}
			return notes;
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
			const auto* piece = object->As<RE::TESObjectARMO>();
			if (Condition::KindWearsOut(*object) && (!piece || ArmorWear::GivesAnything(*piece))) {
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
			} else if (piece && ArmorWear::IsPowerArmor(*piece)) {
				// Armor's filler, see Stock.h.
				powerArmor += chance;
			}
		}

		// Each on its own rows, and nothing stands beside clothing, see
		// Stock.h.
		const Measured measured[]{
			CeilingOf(Trade::kWeapons, Whole(weapons), Whole(rounds) + Whole(thrown), Whole(rows)),
			CeilingOf(Trade::kArmor, Whole(armor), Whole(powerArmor), Whole(rows)),
			CeilingOf(Trade::kClothing, Whole(clothing), 0, Whole(rows)),
		};
		Ceilings out{ measured[0].ceiling, measured[1].ceiling, measured[2].ceiling };

		// A line may name the trader's vendor faction, which the game works
		// out only when something asks, as LevelFor says.
		if (!a_trader.vendorFaction) {
			a_trader.UpdateVendorFaction();
		}
		const auto notes = Listed(a_trader, measured, out);
		if (TraceLog::IsOpen()) {
			TraceLog::Line("menu", "{:s} for the trader list: {:s}", Trader(&a_trader), TraderList::Ids(a_trader));
		}
		TraceLog::Line("menu", "{:s} restocks {:d} rows of weapons, {:d} of ammunition, {:d} of grenades and mines, {:d} of armor, {:d} of power armor and {:d} of clothing out of {:d}{:s}, and repairs {:s}{:s}",
			Trader(&a_trader), Whole(weapons), Whole(rounds), Whole(thrown), Whole(armor), Whole(powerArmor), Whole(clothing),
			Whole(rows), a_now, Repairs(out), notes);
		return out;
	}

	std::size_t Leeway()
	{
		return static_cast<std::size_t>(std::clamp(Settings::iTraderLeeway.GetValue(), 0, MOST_LEEWAY));
	}

	std::size_t FullRows(Trade a_trade)
	{
		const auto rows = static_cast<std::size_t>(std::clamp(Settings::iTraderFullStock.GetValue(), FEWEST_FULL_ROWS, MOST_FULL_ROWS));
		return a_trade == Trade::kWeapons ? rows + GUN_EXTRA_ROWS : rows;
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

		out.worth = SoundWorth(*out.item, out.stack);
		return out;
	}

	std::uint32_t SoundWorth(const RE::BGSInventoryItem& a_item, std::uint32_t a_stack)
	{
		const ItemValue::ScopedSoundPrice sound;
		const auto                        worth = a_item.GetInventoryValue(a_stack, false);
		return worth > 0 ? static_cast<std::uint32_t>(worth) : 0U;
	}

	std::uint32_t LevelFor(RE::Actor& a_trader, const RE::TESBoundObject& a_object)
	{
		// The chest the faction names, as a restock reads it, see Upkeep.cpp.
		// The game works the faction out only when something asks, a trade
		// first, so a script asking before any trade gets it worked out here.
		a_trader.UpdateVendorFaction();
		const auto* faction = a_trader.vendorFaction;
		if (!faction) {
			return 0;
		}
		return CeilingsOf(faction->vendorData.merchantContainer, a_trader, ", worked out for a script").Of(TradeOf(a_object));
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
