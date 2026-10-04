#include "Gameplay/SpawnCondition/Trace.h"

#include <format>
#include <string>
#include <string_view>

namespace SpawnCondition
{
	std::string_view NameOf(const RE::TESForm& a_form, std::string_view a_whenNameless)
	{
		const auto name = RE::TESFullName::GetFullName(a_form);
		return name.empty() ? a_whenNameless : name;
	}

	std::string Describe(const Provenance::Origin& a_origin)
	{
		std::string text;

		// The list by form ID, since a leveled list carries no name and the
		// ID finds it in the editor.
		if (a_origin.supply == Provenance::UNMEASURED) {
			text = "supply unknown";
		} else {
			text = std::format("supply {:.2f} [{:08X}]", a_origin.supply, a_origin.pipeline);
		}

		// 3 cases, not 2: a character with a rank, a character this could
		// not rank, and no character. Merging the middle into the last
		// would hide whether the scale is missing people.
		if (a_origin.care != Provenance::UNMEASURED && a_origin.keeper) {
			text += std::format(", care {:.2f} {:s} [{:08X}]", a_origin.care,
				NameOf(*a_origin.keeper, "unnamed character"), a_origin.keeper->formID);
		} else if (a_origin.keeper) {
			text += std::format(", care unmeasured for {:s} [{:08X}]",
				NameOf(*a_origin.keeper, "unnamed character"), a_origin.keeper->formID);
		} else {
			text += ", no owner";
		}

		return text;
	}

	std::string Stocked(const Restock& a_restock, const StockBand& a_band)
	{
		return std::format("stock of {:s}, who {:s}, rolled in {:.0f}% to {:.0f}%", a_restock.trader,
			a_band.repairs > 0 ? std::format("repairs it to {:d}%", a_band.repairs) : "does not repair it",
			a_band.low * 100.0F, a_band.high * 100.0F);
	}
}
