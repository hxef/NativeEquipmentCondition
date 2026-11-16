#include "UI/Repair/Workbench/Lock.h"

#include "Condition/Condition.h"
#include "Condition/Repair.h"
#include "Core/Settings/Settings.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Repair/RepairPrompt.h"
#include "UI/Repair/Workbench/SpareBox.h"
#include "UI/Repair/Workbench/Spares.h"
#include "UI/Repair/Workbench/Supplies.h"

#include <algorithm>

namespace Workbench
{
	namespace
	{
		// iPerkLock kept between 0 and 100: where a repair starts from with
		// every rank of the perk short.
		std::uint32_t PerkLock()
		{
			constexpr auto full = static_cast<std::int32_t>(Repair::FULL);
			return static_cast<std::uint32_t>(std::clamp(Settings::iPerkLock.GetValue(), 0, full));
		}

		// The condition a repair that takes components can start from with
		// a_short of a_ranks ranks short: an equal share of PerkLock for each,
		// rounded half up to a whole percent as CraftingPerks::Discount rounds,
		// 0 at none. So more ranks short never start lower than fewer.
		std::uint32_t LockFrom(std::uint32_t a_short, std::uint32_t a_ranks)
		{
			const auto lacking = std::min(a_short, a_ranks);
			if (lacking == 0) {
				return 0;
			}
			return (PerkLock() * lacking * 2 + a_ranks) / (2 * a_ranks);
		}
	}

	// -------------------------------------------------------------------
	// The rule
	// -------------------------------------------------------------------

	LockMode LockNow()
	{
		switch (Settings::iRepairLock.GetValue()) {
		case 0:
			return LockMode::kOff;
		case 1:
			return LockMode::kEveryItem;
		default:
			return LockMode::kByMods;
		}
	}

	std::string_view LockWord(LockMode a_mode)
	{
		switch (a_mode) {
		case LockMode::kOff:
			return "off";
		case LockMode::kEveryItem:
			return "every item";
		default:
			return "its mods";
		}
	}

	std::uint32_t LockAsks(std::uint32_t a_asked, std::uint32_t a_ranks, LockMode a_mode)
	{
		switch (a_mode) {
		case LockMode::kOff:
			return 0;
		case LockMode::kEveryItem:
			return a_ranks;
		default:
			return std::min(a_asked, a_ranks);
		}
	}

	std::uint32_t LockAsks(const Priced& a_priced, LockMode a_mode)
	{
		return LockAsks(a_priced.standing.asked, a_priced.standing.ranks, a_mode);
	}

	std::optional<LockHold> HeldBack(const Selection& a_selection, const Priced& a_priced)
	{
		const auto& standing = a_priced.standing;
		const auto  mode = LockNow();
		if (mode == LockMode::kOff || !a_selection.Worn() || a_selection.Trifling() ||
			!Supplies::TakesComponents(a_selection) || !standing.perk || standing.ranks == 0) {
			return std::nullopt;
		}

		const auto asked = LockAsks(a_priced, mode);
		const auto held = std::min(standing.rank, standing.ranks);
		if (asked <= held) {
			return std::nullopt;
		}

		LockHold out;
		out.asked = asked;
		out.lacking = asked - held;
		out.from = LockFrom(out.lacking, standing.ranks);
		if (a_selection.percent >= out.from) {
			return std::nullopt;
		}

		// The lowest rank that would open a repair from here. The rank asked
		// always does, since it leaves nothing short.
		out.need = asked;
		for (auto rank = held + 1; rank < asked; rank++) {
			if (LockFrom(asked - rank, standing.ranks) <= a_selection.percent) {
				out.need = rank;
				break;
			}
		}
		return out;
	}

	std::optional<std::uint32_t> RankOpensFrom(std::uint32_t a_rank, std::uint32_t a_ranks, std::uint32_t a_highest)
	{
		const auto mode = LockNow();
		if (mode == LockMode::kOff || FreeRepairs() || a_ranks == 0 || PerkLock() == 0) {
			return std::nullopt;
		}

		const auto asked = LockAsks(a_highest, a_ranks, mode);
		const auto held = std::min(a_rank, a_ranks);
		const auto from = asked > held ? LockFrom(asked - held, a_ranks) : 0U;
		return std::min(from * Condition::TENTHS_PER_PERCENT, SuppliesOnlyFromTenths());
	}

	// -------------------------------------------------------------------
	// The press
	// -------------------------------------------------------------------

	bool LockStops(const Selection& a_selection, const Priced& a_priced)
	{
		if (const auto mode = LockNow(); mode != LockMode::kOff) {
			const auto& standing = a_priced.standing;
			TraceLog::Line("menu", "Workbench reads {:s} as asking {:s} {:d} by {:s}, the player {:d} of {:d} ranks in",
				a_selection.Name(), standing.Name(), LockAsks(a_priced, mode), LockWord(mode), standing.rank, standing.ranks);
		}
		if (const auto held = HeldBack(a_selection, a_priced)) {
			TraceLog::Line("menu", "Workbench holds back {:s} at {:s}%, which repairs from {:d}% with {:d} ranks short, {:s} {:d} would repair it now",
				a_selection.Name(), Condition::CardText(a_selection.tenths), held->from, held->lacking, a_priced.standing.Name(), held->need);
			const bool sparesFree = !Settings::bLockSpares.GetValue();
			if (sparesFree && CarriesSpare(a_selection)) {
				TraceLog::Line("menu", "Workbench opened the spares of {:s}, the repair lock holds it back", a_selection.Name());
				AskWhichSpare(*a_selection.object, a_selection.extra, a_priced);
				return true;
			}
			// The corner names a spare only where another copy could be one. A
			// unique weapon such as the Cryolator is one only at
			// iUnscrappableSpares 2.
			const auto said = Text::RepairLocked(a_priced.standing.Name(), held->need, Condition::CardText(a_selection.tenths),
				sparesFree && SpareCanExist(a_selection), Settings::bVendorRepair.GetValue());
			RE::SendHUDMessage::ShowHUDMessage(said.c_str(), nullptr, true, true);
			RE::UIUtils::PlayMenuSound(RepairPrompt::REFUSED_SOUND);
			TraceLog::Line("menu", "Workbench said the repair lock holds {:s} back, {:s}", a_selection.Name(),
				Settings::bSpareRepair.GetValue() && !sparesFree ? "spares too, since bLockSpares is on" : "with no spare");
			return true;
		}
		return false;
	}

	// -------------------------------------------------------------------
	// NEC.log
	// -------------------------------------------------------------------

	void SayLock()
	{
		const auto mode = LockNow();
		if (mode == LockMode::kOff) {
			REX::INFO("Workbench repairs need no crafting perk rank, since iRepairLock is 0.");
			return;
		}
		REX::INFO("Workbench repairs that take components need {:s}. With every rank of the perk short a repair starts from {:d}%, and each rank short is an equal share of that.",
			mode == LockMode::kEveryItem ? "the last rank of each item's crafting perk" : "the crafting perk rank each item's mods ask for",
			PerkLock());
	}
}
