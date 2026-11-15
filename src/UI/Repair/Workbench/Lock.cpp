#include "UI/Repair/Workbench/Lock.h"

#include "Condition/Repair.h"
#include "Core/Settings.h"
#include "UI/Repair/Workbench/Supplies.h"

#include <algorithm>
#include <array>

namespace Workbench
{
	namespace
	{
		// The sliders by ranks short, 1, 2, and 3 or more. 4 ranks short or
		// more read the 3rd, as 3 do.
		constexpr std::array<Settings::Live<std::int32_t>*, 3> SHORT_SLIDERS{ &Settings::iLock1RankShort,
			&Settings::iLock2RanksShort, &Settings::iLock3RanksShort };

		// The condition a repair that takes components can start from with
		// a_short ranks short: 0 at none, else the highest of the sliders up
		// to a_short's, 3 at most, each kept between 0 and 100. So more ranks
		// short never start lower than fewer, whatever the sliders say.
		std::uint32_t LockFrom(std::uint32_t a_short)
		{
			constexpr auto full = static_cast<std::int32_t>(Repair::FULL);
			const auto     count = std::min<std::size_t>(a_short, SHORT_SLIDERS.size());

			std::uint32_t from = 0;
			for (std::size_t at = 0; at < count; at++) {
				from = std::max(from, static_cast<std::uint32_t>(std::clamp(SHORT_SLIDERS[at]->GetValue(), 0, full)));
			}
			return from;
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

	std::optional<Held> HeldBack(const Selection& a_selection, const Priced& a_priced)
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

		Held out;
		out.asked = asked;
		out.lacking = asked - held;
		out.from = LockFrom(out.lacking);
		if (a_selection.percent >= out.from) {
			return std::nullopt;
		}

		// The lowest rank that would open a repair from here. The rank asked
		// always does, since it leaves nothing short.
		out.need = asked;
		for (auto rank = held + 1; rank < asked; rank++) {
			if (LockFrom(asked - rank) <= a_selection.percent) {
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
		REX::INFO("Workbench repairs that take components need {:s}: 1 rank short they start from {:d}%, 2 from {:d}%, 3 or more from {:d}%.",
			mode == LockMode::kEveryItem ? "the last rank of each item's crafting perk" : "the crafting perk rank each item's mods ask for",
			LockFrom(1), LockFrom(2), LockFrom(3));
	}
}
