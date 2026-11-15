#include "UI/Repair/Workbench/Lock.h"

#include "Condition/Repair.h"
#include "Core/Settings.h"
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

	std::uint32_t LockAsks(const Priced& a_priced, LockMode a_mode)
	{
		const auto& standing = a_priced.standing;
		switch (a_mode) {
		case LockMode::kOff:
			return 0;
		case LockMode::kEveryItem:
			return standing.ranks;
		default:
			return std::min(standing.asked, standing.ranks);
		}
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
