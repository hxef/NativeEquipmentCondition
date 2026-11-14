#include "UI/Repair/Workbench/Supplies.h"

#include "Condition/Repair.h"
#include "Core/Settings.h"

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <string_view>

namespace Workbench::Supplies
{
	namespace
	{
		// A supply: its name for NEC.log and its component's ID in
		// Fallout4.esm. A component keeps no editor ID in memory, so it goes
		// by its ID.
		struct Supply
		{
			std::string_view name;
			RE::TESFormID    id;
		};

		constexpr Supply OIL{ "Oil", 0x01FAB4 };            // c_Oil
		constexpr Supply ADHESIVE{ "Adhesive", 0x01FAA5 };  // c_Adhesive
		constexpr Supply CLOTH{ "Cloth", 0x1223C7 };        // c_Cloth

		// The supplies of each kind, in the order a box lists them.
		constexpr std::array<Supply, 2> WEAPON_SUPPLIES{ OIL, CLOTH };
		constexpr std::array<Supply, 2> ARMOR_SUPPLIES{ ADHESIVE, CLOTH };
		constexpr std::array<Supply, 3> ALL_SUPPLIES{ OIL, ADHESIVE, CLOTH };

		// The component as the load order holds it now, or nothing.
		[[nodiscard]] const RE::BGSComponent* ComponentOf(const Supply& a_supply)
		{
			return g_dataHandler ? g_dataHandler->LookupForm<RE::BGSComponent>(a_supply.id, "Fallout4.esm") : nullptr;
		}
	}

	// -------------------------------------------------------------------
	// The rule
	// -------------------------------------------------------------------

	std::uint32_t Count()
	{
		return static_cast<std::uint32_t>(std::clamp(Settings::iRepairSupplies.GetValue(), 0, MOST));
	}

	std::uint32_t From()
	{
		constexpr auto full = static_cast<std::int32_t>(Repair::FULL);
		return static_cast<std::uint32_t>(std::clamp(Settings::iSuppliesOnlyFrom.GetValue(), 0, full));
	}

	bool TakesComponents(std::uint32_t a_percent, Condition::Kind a_kind)
	{
		return a_percent < From() || Of(a_kind).empty();
	}

	// -------------------------------------------------------------------
	// The bill
	// -------------------------------------------------------------------

	std::vector<Materials::Part> Of(Condition::Kind a_kind)
	{
		std::vector<Materials::Part> out;
		const auto                   count = Count();
		if (count == 0) {
			return out;
		}
		for (const auto& supply : a_kind == Condition::Kind::kArmor ? ARMOR_SUPPLIES : WEAPON_SUPPLIES) {
			if (const auto* component = ComponentOf(supply)) {
				out.push_back({ component, count });
			}
		}
		return out;
	}

	std::vector<Materials::Part> Add(const std::vector<Materials::Part>& a_bill, Condition::Kind a_kind)
	{
		auto out = Of(a_kind);
		for (const auto& part : a_bill) {
			const auto same = std::ranges::find(out, part.component, &Materials::Part::component);
			if (same != out.end()) {
				same->count += part.count;
			} else {
				out.push_back(part);
			}
		}
		return out;
	}

	// -------------------------------------------------------------------
	// NEC.log
	// -------------------------------------------------------------------

	void Say()
	{
		const auto count = Count();
		if (count == 0) {
			REX::INFO("Workbench repairs take no supplies, since iRepairSupplies is 0.");
			return;
		}
		const auto from = From();
		REX::INFO("Workbench repairs take {0:d} Oil and {0:d} Cloth for a weapon and {0:d} Adhesive and {0:d} Cloth for armor, {1:s}.",
			count, from < Repair::FULL ? std::format("and from {:d}% condition nothing else", from) : std::string{ "on top of the components, since iSuppliesOnlyFrom is 100" });
	}

	void SayMissing()
	{
		for (const auto& supply : ALL_SUPPLIES) {
			if (!ComponentOf(supply)) {
				REX::INFO("Workbench repairs take no {:s}, since Fallout4.esm has no component at {:06X}.",
					supply.name, supply.id);
			}
		}
	}
}
