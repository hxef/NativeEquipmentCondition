#include "UI/Repair/Workbench/SpareMods.h"

#include "Condition/Condition.h"
#include "Condition/Repair.h"
#include "Core/Settings/Settings.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog/TraceLog.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <utility>

namespace Workbench
{
	namespace
	{
		using Condition::FULL_TENTHS;
		using Condition::MOST_WORN_TENTHS;
		using Condition::TENTHS_PER_PERCENT;

		// The most mods the corner line names. A longer line names these
		// and counts the rest.
		constexpr std::size_t NAMED_KEPT = 3;

		// The form IDs of the mods of a_order not scrapped, a plan.
		[[nodiscard]] std::vector<std::uint32_t> PlanOf(std::span<const Materials::LooseMod> a_order, const std::vector<bool>& a_scrapped)
		{
			std::vector<std::uint32_t> out;
			for (std::size_t i = 0; i < a_order.size(); i++) {
				if (!a_scrapped[i]) {
					out.push_back(a_order[i].FormID());
				}
			}
			return out;
		}

		// "Suppressor, Standard Sights", or "no mod", for the trace.
		[[nodiscard]] std::string Names(std::span<const std::string> a_names)
		{
			std::string out;
			for (const auto& name : a_names) {
				out += out.empty() ? name : ", " + name;
			}
			return out.empty() ? std::string{ "no mod" } : out;
		}

		// The tenth a_worth reaches from a_item's card number on the curve in
		// Repair.h, rounded down and kept from a_low to a_low + 9. a_low is
		// the whole steps a_worth pays, so its whole part is always theirs.
		[[nodiscard]] std::uint32_t Reach(const Selection& a_item, const Priced& a_priced, std::uint32_t a_worth, std::uint32_t a_low)
		{
			const auto bill = BillWorth(a_priced);
			if (!(bill > 0.0)) {
				return a_low;
			}
			// What the item still owes once the worth is spent, as Debt counts it.
			const auto owed = static_cast<double>(Repair::DebtTenths(a_item.tenths, a_priced.multiple)) - a_worth / bill;
			const auto reached = Repair::Reached(static_cast<float>(owed), a_priced.multiple);
			const auto tenths = std::floor(static_cast<double>(reached) * TENTHS_PER_PERCENT);
			return static_cast<std::uint32_t>(std::clamp(tenths, static_cast<double>(a_low), static_cast<double>(a_low + TENTHS_PER_PERCENT - 1)));
		}

		// LandingOf for a_worth paying a_gain whole steps.
		[[nodiscard]] std::uint32_t Landing(const Selection& a_item, const Priced& a_priced, std::uint32_t a_worth, std::uint32_t a_gain,
			bool* a_mended)
		{
			const auto steps = std::min(std::min(a_item.tenths, FULL_TENTHS) + a_gain * TENTHS_PER_PERCENT, FULL_TENTHS);
			const bool tenth = steps < FULL_TENTHS && Condition::Decimals() != Condition::CardDecimals::kOff;
			const auto landing = tenth ? std::min(Reach(a_item, a_priced, a_worth, steps), MOST_WORN_TENTHS) : steps;
			const bool mended = landing < FULL_TENTHS && landing >= FreeFromTenths();
			if (a_mended) {
				*a_mended = mended;
			}
			return mended ? FULL_TENTHS : landing;
		}
	}

	// -----------------------------------------------------------------------
	// The setting and the mods
	// -----------------------------------------------------------------------

	KeepMods KeepModsNow()
	{
		switch (Settings::iKeepSpareMods.GetValue()) {
		case std::to_underlying(KeepMods::kCheapest):
			return KeepMods::kCheapest;
		case std::to_underlying(KeepMods::kPriciest):
			return KeepMods::kPriciest;
		case std::to_underlying(KeepMods::kChooseUse):
			return KeepMods::kChooseUse;
		case std::to_underlying(KeepMods::kChooseKeep):
			return KeepMods::kChooseKeep;
		default:
			return KeepMods::kOff;
		}
	}

	std::vector<std::uint32_t> ReadMods(Spare& a_spare, const RE::ExtraDataList* a_extra, std::span<const std::uint32_t> a_kept)
	{
		a_spare.mods = Materials::LooseMods(a_extra);
		std::ranges::stable_sort(a_spare.mods, [](const Materials::LooseMod& a_left, const Materials::LooseMod& a_right) {
			if (a_left.caps != a_right.caps) {
				return a_left.caps < a_right.caps;
			}
			return a_left.name != a_right.name ? a_left.name < a_right.name : a_left.FormID() < a_right.FormID();
		});

		// Only a mod that comes off can be kept, so a plan never leaves a
		// legendary mod out of the scrap.
		std::vector<std::uint32_t> out;
		auto                       left = a_kept.empty() ? std::vector<Materials::LooseMod>{} : a_spare.mods;
		for (const auto id : a_kept) {
			if (const auto mod = std::ranges::find(left, id, &Materials::LooseMod::FormID); mod != left.end()) {
				a_spare.kept.push_back(*mod);
				out.push_back(id);
				left.erase(mod);
			}
		}
		return out;
	}

	// -----------------------------------------------------------------------
	// Where a plan brings the item
	// -----------------------------------------------------------------------

	std::uint32_t LandingOf(const Spare& a_spare, const Selection& a_item, const Priced& a_priced, bool* a_mended)
	{
		return Landing(a_item, a_priced, a_spare.worth, a_spare.gain, a_mended);
	}

	std::uint32_t RestoredTenths(const Selection& a_item, const Priced& a_priced, std::uint32_t a_worth, std::uint32_t a_gain)
	{
		if (Condition::Decimals() == Condition::CardDecimals::kOff) {
			return a_gain * TENTHS_PER_PERCENT;
		}
		return Landing(a_item, a_priced, a_worth, a_gain, nullptr) - std::min(a_item.tenths, FULL_TENTHS);
	}

	std::int32_t ShownTenths(const Spare& a_spare, const Selection& a_item, const Priced& a_priced)
	{
		const auto landing = LandingOf(a_spare, a_item, a_priced, nullptr);
		if (landing >= FULL_TENTHS || Condition::Decimals() != Condition::CardDecimals::kOff) {
			return static_cast<std::int32_t>(landing);
		}
		// At Off the steps before LEAST_GAIN, so the box never claims more
		// than the worth pays.
		const auto low = a_item.tenths + Covered(a_priced, a_item.tenths, a_spare.worth) * TENTHS_PER_PERCENT;
		return static_cast<std::int32_t>(Reach(a_item, a_priced, a_spare.worth, low));
	}

	// -----------------------------------------------------------------------
	// The automatic plans
	// -----------------------------------------------------------------------

	std::vector<std::uint32_t> AutoKept(const Selection& a_item, const Priced& a_priced, const Spare& a_whole, KeepMods a_setting)
	{
		const bool priciest = a_setting == KeepMods::kPriciest;
		if ((a_setting != KeepMods::kCheapest && !priciest) || a_whole.mods.empty()) {
			return {};
		}
		auto order = a_whole.mods;
		if (priciest) {
			std::ranges::reverse(order);
		}

		// The card after the box, so the plan lands where the whole spare
		// does, which the box of spares showed. At CND decimals Off the
		// landing is a whole step.
		const auto level = [&](const Spare& a_spare) { return LandingOf(a_spare, a_item, a_priced, nullptr); };

		std::vector<bool> scrapped(order.size(), false);
		const auto        goal = level(a_whole);
		auto              plan = SpareOn(a_item, a_priced, a_whole.extra, PlanOf(order, scrapped));
		const auto        reached = [&] { return !plan || level(*plan) >= goal; };

		// The frame alone first, then 1 mod at a time.
		std::vector<std::size_t> taken;
		std::vector<std::size_t> passed;
		for (std::size_t i = 0; i < order.size() && !reached(); i++) {
			scrapped[i] = true;
			auto worked = SpareOn(a_item, a_priced, a_whole.extra, PlanOf(order, scrapped));
			if (worked && worked->worth == plan->worth) {
				scrapped[i] = false;
				passed.push_back(i);
				continue;
			}
			plan = std::move(worked);
			taken.push_back(i);
		}
		// Mods that add nothing alone can add worth together, through the
		// rounding of a count, so the condition the whole spare reaches is
		// always reached in the end.
		for (const auto i : passed) {
			if (reached()) {
				break;
			}
			scrapped[i] = true;
			plan = SpareOn(a_item, a_priced, a_whole.extra, PlanOf(order, scrapped));
			taken.push_back(i);
		}

		std::vector<std::string> scraps;
		std::vector<std::string> keeps;
		for (const auto i : taken) {
			scraps.push_back(order[i].name);
		}
		for (std::size_t i = 0; i < order.size(); i++) {
			if (!scrapped[i]) {
				keeps.push_back(order[i].name);
			}
		}
		TraceLog::Line("menu", "Workbench scraps {:s} of {:s}, the {:s} first, and keeps {:s}", Names(scraps), a_whole.name,
			priciest ? "priciest" : "cheapest", Names(keeps));
		return PlanOf(order, scrapped);
	}

	// -----------------------------------------------------------------------
	// What the player is told
	// -----------------------------------------------------------------------

	void TraceSplit(const Spare& a_whole)
	{
		if (!TraceLog::IsOpen()) {
			return;
		}
		std::string listed;
		for (const auto& mod : a_whole.mods) {
			listed += std::format(", {:s} ({:d} caps)", mod.name, mod.caps);
		}
		TraceLog::Line("menu", "Workbench splits {:s} at {:s}%: {:d} of its mods can come off{:s}, and a mod with keyword {:08X} never does",
			a_whole.name, Condition::CardText(a_whole.tenths), a_whole.mods.size(), listed, Materials::NullModKeyword());
	}

	void TraceHandedBack(const Spare& a_used)
	{
		if (a_used.kept.empty() || !TraceLog::IsOpen()) {
			return;
		}
		std::vector<std::string> items;
		for (const auto& mod : a_used.kept) {
			items.push_back(std::format("{:s} ({:08X}, rank {:d})", RE::TESFullName::GetFullName(*mod.item), mod.item->GetFormID(), mod.rank));
		}
		TraceLog::Line("menu", "Workbench handed back {:d} loose mods from {:s}: {:s}", a_used.kept.size(), a_used.name, Names(items));
	}

	std::string KeptLine(std::span<const Materials::LooseMod> a_kept)
	{
		std::vector<const Materials::LooseMod*> priciest;
		for (const auto& mod : a_kept) {
			priciest.push_back(&mod);
		}
		std::ranges::stable_sort(priciest, [](const Materials::LooseMod* a_left, const Materials::LooseMod* a_right) {
			if (a_left->caps != a_right->caps) {
				return a_left->caps > a_right->caps;
			}
			return a_left->name != a_right->name ? a_left->name < a_right->name : a_left->FormID() < a_right->FormID();
		});

		std::vector<std::string> names;
		for (std::size_t i = 0; i < priciest.size() && i < NAMED_KEPT; i++) {
			names.push_back(priciest[i]->name);
		}
		if (names.empty()) {
			return {};
		}
		return Text::SpareKept(names, priciest.size() - names.size());
	}
}
