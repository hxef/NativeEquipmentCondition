#include "Papyrus/Natives.h"

#include "Condition/Condition.h"
#include "Condition/CraftingPerks/CraftingPerks.h"
#include "Condition/Materials/Materials.h"
#include "Condition/Materials/Yield.h"
#include "Condition/Repair.h"
#include "Core/TraceLog.h"
#include "Papyrus/Items.h"
#include "UI/Repair/VendorRepair/VendorRepair.h"
#include "UI/Repair/Workbench/Workbench.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// The repair economics: what the bench and a trader would charge and give,
// each from the code their menus price with. Nothing is taken or paid.
namespace Papyrus
{
	namespace
	{
		// -------------------------------------------------------------------
		// The item and the answers
		// -------------------------------------------------------------------

		// What a call takes from an item under its holder's lock, for the work
		// after it. The counted list keeps the stack's list alive once the lock
		// is let go, and goes when the call returns.
		struct Copied
		{
			RE::TESBoundObject*                    base{ nullptr };
			RE::BSTSmartPointer<RE::ExtraDataList> extra;
			std::uint32_t                          percent{ Repair::FULL };
			std::uint32_t                          worth{ 0 };
			bool                                   inventory{ false };
			bool                                   player{ false };
		};

		// What a_item names, with its sound price for a_worth, or nothing for
		// None, a gone item and gear NEC gives no condition.
		std::optional<Copied> CopyOf(const Item& a_item, bool a_worth = false)
		{
			Holding     held{ a_item };
			const auto* found = held.found();
			if (!found || !Tracked(*found)) {
				return std::nullopt;
			}
			Copied out;
			out.base = found->base;
			out.extra.reset(found->extra);
			const auto percent = Condition::Percent(*found->base, found->extra);
			out.percent = percent ? static_cast<std::uint32_t>(*percent) : Repair::FULL;
			out.inventory = found->entry != nullptr;
			out.player = found->holder && found->holder->IsPlayerRef();
			if (a_worth && found->entry) {
				out.worth = VendorRepair::SoundWorth(*found->entry, found->stack);
			}
			return out;
		}

		// a_form as an item NEC gives a condition, or null.
		RE::TESBoundObject* TrackedBase(RE::TESForm* a_form)
		{
			auto* base = a_form ? a_form->As<RE::TESBoundObject>() : nullptr;
			return base && Condition::WearsOut(*base) ? base : nullptr;
		}

		// A percent a script asks for, kept between 0 and 100.
		std::uint32_t Level(std::int32_t a_percent)
		{
			return static_cast<std::uint32_t>(std::clamp(a_percent, 0, static_cast<std::int32_t>(Repair::FULL)));
		}

		std::int32_t Result(std::int32_t a_value)
		{
			TraceLog::Papyrus::Line("result", "{:d}", a_value);
			return a_value;
		}

		// A bill as the script's Cost array, with its trace line, None for
		// nothing.
		std::optional<std::vector<Cost>> Result(const std::optional<std::vector<Materials::Part>>& a_parts)
		{
			if (!a_parts) {
				TraceLog::Papyrus::Line("result", "none");
				return std::nullopt;
			}
			std::vector<Cost> out;
			std::string       text;
			for (const auto& part : *a_parts) {
				if (!part.component) {
					continue;
				}
				Cost cost;
				cost.insert("object", part.component);
				cost.insert("count", static_cast<std::int32_t>(part.count));
				out.push_back(std::move(cost));
				std::format_to(std::back_inserter(text), "{:s}{:d} {:s}", text.empty() ? "" : ", ", part.count, Hex(part.component));
			}
			TraceLog::Papyrus::Line("result", "{:d} costs{:s}{:s}", out.size(), text.empty() ? "" : ": ", text);
			return out;
		}

		// A trader's functions answer as for a trader who repairs nothing
		// while trader repairs do not run.
		bool TradersRepair()
		{
			if (FeatureRuns("VendorRepair")) {
				return true;
			}
			TraceLog::Papyrus::Line("refused", "trader repairs are off");
			return false;
		}
	}

	// -------------------------------------------------------------------
	// The 8 functions
	// -------------------------------------------------------------------

	std::int32_t GetFreeMendAbove(std::monostate)
	{
		Log::Calling("GetFreeMendAbove");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetFreeMendAbove");
		return Result(static_cast<std::int32_t>(Workbench::FreeAbove()));
	}

	std::int32_t GetModLockBelow(std::monostate)
	{
		Log::Calling("GetModLockBelow");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetModLockBelow");
		return Result(static_cast<std::int32_t>(Workbench::ModifyFloor()));
	}

	std::int32_t GetRepairDiscount(std::monostate, RE::TESForm* a_base)
	{
		Log::Calling("GetRepairDiscount");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetRepairDiscount base={:s}", Hex(a_base));
		const auto* base = TrackedBase(a_base);
		if (!base) {
			return Result(0);
		}
		// The perk that prices the plain item, by its own recipes.
		const auto standing = CraftingPerks::Of(*base, nullptr, Materials::BillOfLines(*base, nullptr));
		return Result(static_cast<std::int32_t>(CraftingPerks::Discount(standing.rank, standing.ranks)));
	}

	std::optional<std::vector<Cost>> GetRepairCost(std::monostate, Item a_item, std::int32_t a_toPercent)
	{
		Log::Calling("GetRepairCost");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetRepairCost item={:s} to={:d}", Shown(a_item), a_toPercent);
		const auto item = CopyOf(a_item);
		if (!item) {
			return Result(std::nullopt);
		}
		return Result(Workbench::BillFor(*item->base, item->extra.get(), item->percent, Level(a_toPercent)));
	}

	std::optional<std::vector<Cost>> GetScrapYield(std::monostate, Item a_item)
	{
		Log::Calling("GetScrapYield");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetScrapYield item={:s}", Shown(a_item));
		const auto item = CopyOf(a_item);
		if (!item) {
			return Result(std::nullopt);
		}
		// While worn scrapping does not run, the bench scraps as vanilla.
		const auto percent = FeatureRuns("Scrapping") ? item->percent : Repair::FULL;
		return Result(Materials::Yield(*item->base, item->extra.get(), percent, Materials::SCRAPPED_TENTHS));
	}

	std::int32_t GetSpareGain(std::monostate, Item a_item, Item a_spare)
	{
		Log::Calling("GetSpareGain");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetSpareGain item={:s} spare={:s}", Shown(a_item), Shown(a_spare));
		const auto item = CopyOf(a_item);
		const auto spare = item ? CopyOf(a_spare) : std::nullopt;
		if (!spare || !item->player || !spare->player || spare->base != item->base) {
			TraceLog::Papyrus::Line("refused", "both must be the player's, of 1 weapon or armor");
			return Result(-1);
		}
		const auto gain = Workbench::SpareGain(*item->base, item->extra.get(), spare->extra.get());
		return Result(gain ? static_cast<std::int32_t>(*gain) : -1);
	}

	std::int32_t GetTraderRepairLevel(std::monostate, RE::Actor* a_trader, RE::TESForm* a_base)
	{
		Log::Calling("GetTraderRepairLevel");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetTraderRepairLevel trader={:s} base={:s}", Hex(a_trader), Hex(a_base));
		const auto* base = TrackedBase(a_base);
		if (!a_trader || !base || !TradersRepair()) {
			return Result(0);
		}
		return Result(static_cast<std::int32_t>(VendorRepair::LevelFor(*a_trader, *base)));
	}

	std::int32_t GetTraderRepairPrice(std::monostate, RE::Actor* a_trader, Item a_item, std::int32_t a_toPercent)
	{
		Log::Calling("GetTraderRepairPrice");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetTraderRepairPrice trader={:s} item={:s} to={:d}", Hex(a_trader), Shown(a_item),
			a_toPercent);
		if (!a_trader || !TradersRepair()) {
			return Result(-1);
		}
		const auto item = CopyOf(a_item, true);
		if (!item || !item->inventory) {
			return Result(-1);
		}
		const auto price = VendorRepair::PriceFor(*a_trader, *item->base, item->percent, item->worth, Level(a_toPercent));
		return Result(price ? static_cast<std::int32_t>(*price) : -1);
	}
}
