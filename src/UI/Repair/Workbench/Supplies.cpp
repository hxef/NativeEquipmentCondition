#include "UI/Repair/Workbench/Supplies.h"

#include "Condition/Condition.h"
#include "Condition/Repair.h"
#include "Core/Settings/Settings.h"

#include <algorithm>
#include <array>
#include <format>
#include <span>
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

		// The components that mark a melee weapon's mod as powered, by their
		// IDs in Fallout4.esm: a heating coil, a shock or stun pack, the
		// rocket of a bat.
		constexpr std::array<RE::TESFormID, 2> POWER_COMPONENTS{
			0x01FA9B,  // c_Circuitry
			0x01FAB3,  // c_NuclearMaterial
		};

		// The supplies of each set, in the order a box lists them. Oil
		// keeps what fires, runs on a motor or a power source going,
		// adhesive mends a plain blade, club or fist, and cloth is the rag
		// for a gun's action and the patch for armor and clothing.
		constexpr std::array<Supply, 2> MECHANICAL_SUPPLIES{ OIL, CLOTH };
		constexpr std::array<Supply, 1> PLAIN_SUPPLIES{ ADHESIVE };
		constexpr std::array<Supply, 2> ARMOR_SUPPLIES{ ADHESIVE, CLOTH };
		constexpr std::array<Supply, 3> ALL_SUPPLIES{ OIL, ADHESIVE, CLOTH };

		// The component of Fallout4.esm at a_id as the load order holds it
		// now, or nothing.
		[[nodiscard]] const RE::BGSComponent* ComponentOf(RE::TESFormID a_id)
		{
			return g_dataHandler ? g_dataHandler->LookupForm<RE::BGSComponent>(a_id, "Fallout4.esm") : nullptr;
		}

		[[nodiscard]] const RE::BGSComponent* ComponentOf(const Supply& a_supply)
		{
			return ComponentOf(a_supply.id);
		}

		// A gun, or a melee weapon with a motor or a power source: a copy
		// with the Automatic flag, such as the Ripper, or one with a mod
		// built from Circuitry or Nuclear Material, such as a Heated Super
		// Sledge or a baton with a shock mod. Read off the copy, so a weapon
		// of another mod follows the same rule, and a plain Super Sledge or a
		// Baseball Bat with large blades is neither.
		[[nodiscard]] bool Mechanical(const SelectedItem::Item& a_item)
		{
			const auto* weapon = a_item.object ? a_item.object->As<RE::TESObjectWEAP>() : nullptr;
			if (!weapon) {
				return false;
			}
			if (weapon->IsGunWeapon() ||
				Condition::InstanceDataOf(a_item.extra, weapon->weaponData).flags.any(RE::WEAPON_FLAGS::kAutomatic)) {
				return true;
			}
			std::array<const RE::BGSComponent*, POWER_COMPONENTS.size()> power{};
			std::ranges::transform(POWER_COMPONENTS, power.begin(), [](RE::TESFormID a_id) { return ComponentOf(a_id); });
			return Materials::ModsAskFor(a_item.extra, power);
		}

		// The set a_item takes: oil and cloth for a mechanical weapon,
		// adhesive alone for any other weapon, adhesive and cloth for armor
		// and clothing.
		[[nodiscard]] std::span<const Supply> SuppliesFor(const SelectedItem::Item& a_item)
		{
			if (a_item.Armor()) {
				return ARMOR_SUPPLIES;
			}
			if (Mechanical(a_item)) {
				return MECHANICAL_SUPPLIES;
			}
			return PLAIN_SUPPLIES;
		}
	}

	// -------------------------------------------------------------------
	// The rule
	// -------------------------------------------------------------------

	std::uint32_t Count()
	{
		return static_cast<std::uint32_t>(std::clamp(Settings::iRepairSupplies.GetValue(), 0, MOST));
	}

	std::uint32_t FromTenths()
	{
		return Repair::CardTenthsFrom(Settings::fSuppliesOnlyFrom.GetValue());
	}

	bool AnyHeld()
	{
		return std::ranges::any_of(ALL_SUPPLIES, [](const Supply& a_supply) { return ComponentOf(a_supply) != nullptr; });
	}

	bool TakesComponents(const SelectedItem::Item& a_item)
	{
		return a_item.tenths < FromTenths() || Of(a_item).empty();
	}

	// -------------------------------------------------------------------
	// The bill
	// -------------------------------------------------------------------

	std::vector<Materials::Part> Of(const SelectedItem::Item& a_item)
	{
		std::vector<Materials::Part> out;
		const auto                   count = Count();
		if (count == 0) {
			return out;
		}
		for (const auto& supply : SuppliesFor(a_item)) {
			if (const auto* component = ComponentOf(supply)) {
				out.push_back({ component, count });
			}
		}
		return out;
	}

	std::vector<Materials::Part> Add(const std::vector<Materials::Part>& a_bill, const SelectedItem::Item& a_item)
	{
		auto out = Of(a_item);
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
		// At CND decimals Off condition is written in whole percents, so the
		// line names the one the slider starts from as well.
		const auto from = FromTenths();
		const auto value = Settings::fSuppliesOnlyFrom.GetValue();
		const auto alone = Condition::Decimals() == Condition::CardDecimals::kOff ?
		                       std::format("and from {:g}% condition, so from {:d}% in whole numbers, nothing else", value, Repair::PercentFrom(from)) :
		                       std::format("and from {:s}% condition, nothing else", Condition::PointText(from));
		REX::INFO("Workbench repairs take {0:d} Oil and {0:d} Cloth for a gun, an automatic melee weapon or one with a mod built from Circuitry or Nuclear Material, {0:d} Adhesive for any other weapon, {0:d} Adhesive and {0:d} Cloth for armor, {1:s}.",
			count, from < Condition::FULL_TENTHS ? alone : std::format("on top of the components, since fSuppliesOnlyFrom is {:g}", value));
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
