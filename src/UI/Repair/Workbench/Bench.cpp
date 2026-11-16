#include "UI/Repair/Workbench/Bench.h"

#include "Condition/Repair.h"
#include "Core/BenchRow.h"
#include "Core/Settings/Settings.h"
#include "UI/Repair/Workbench/Inspect.h"
#include "UI/Repair/Workbench/Supplies.h"

#include <algorithm>

namespace Workbench
{
	std::uint32_t FreeFromTenths()
	{
		return Repair::CardTenthsFrom(Settings::fFreeMendFrom.GetValue());
	}

	bool SuppliesAlone()
	{
		return Supplies::Count() > 0 && !FreeRepairs() && Supplies::FromTenths() < FreeFromTenths() && Supplies::AnyHeld();
	}

	std::uint32_t SuppliesOnlyFromTenths()
	{
		return SuppliesAlone() ? Supplies::FromTenths() : FreeFromTenths();
	}

	std::uint32_t ModifyFloor()
	{
		constexpr auto full = static_cast<std::int32_t>(Repair::FULL);
		return static_cast<std::uint32_t>(std::clamp(Settings::iModdingFrom.GetValue(), 0, full));
	}

	bool FreeRepairs()
	{
		// Also true for NaN, which Scaled reads as 0 too.
		return !(Settings::fBenchCostMult.GetValue() > 0.0F);
	}

	Selection Selected(RE::ExamineMenu* a_menu)
	{
		const auto* row = a_menu && !a_menu->inspectMode ? BenchRow::Highlighted(*a_menu) : nullptr;
		return row ? Selection{ SelectedItem::Read(*row) } : Selection{};
	}

	std::int32_t PipboyRepair()
	{
		const auto setting = Settings::iPipboyRepair.GetValue();
		if (setting == 1) {
			return Settings::bSpareRepair.GetValue() ? 1 : 0;
		}
		return setting == 2 ? 2 : 0;
	}

	bool PipboyInspect(RE::ExamineMenu* a_menu)
	{
		const auto* ui = RE::UI::GetSingleton();
		return a_menu && ui && a_menu->inspectMode && !a_menu->inspectingFeaturedItem && !a_menu->workbenchRef.get() &&
		       ui->GetMenuOpen<RE::PipboyMenu>() && !ui->GetMenuOpen<RE::ContainerMenu>() && !ui->GetMenuOpen<RE::BarterMenu>();
	}

	Selection Inspected(RE::ExamineMenu* a_menu)
	{
		const auto* row = PipboyInspect(a_menu) ? BenchRow::Highlighted(*a_menu) : nullptr;
		return row ? Selection{ SelectedItem::Read(*row) } : Selection{};
	}

	Selection Repairing(RE::ExamineMenu* a_menu)
	{
		if (PipboyInspect(a_menu)) {
			return PipboyRepair() == 2 ? Inspected(a_menu) : Selection{};
		}
		return Selected(a_menu);
	}

	bool RepairsIn(RE::ExamineMenu* a_menu)
	{
		if (!a_menu) {
			return false;
		}
		if (PipboyInspect(a_menu)) {
			return Settings::NecOn() && PipboyRepair() > 0 && Repairs() && Inspect::KeyLive();
		}
		return !a_menu->inspectMode && BenchRepairs();
	}

	std::optional<Condition::Kind> WorksOn(RE::ExamineMenu* a_menu)
	{
		const auto* bench = a_menu ? a_menu->workbenchRef.get() : nullptr;
		const auto* base = bench ? bench->GetObjectReference() : nullptr;
		const auto* furniture = base ? base->As<RE::TESFurniture>() : nullptr;
		if (!furniture) {
			return std::nullopt;
		}

		switch (furniture->wbData.type.get()) {
		case RE::WorkbenchData::Type::kWeapons:
			return Condition::Kind::kWeapon;
		case RE::WorkbenchData::Type::kArmor:
			return Condition::Kind::kArmor;
		default:
			return std::nullopt;
		}
	}

	RE::ExamineMenu* OpenBench()
	{
		const auto* ui = RE::UI::GetSingleton();
		const auto  menu = ui ? ui->GetMenu<RE::ExamineMenu>() : nullptr;
		return menu.get();
	}

	void RebuildModdedItem(RE::ExamineMenu* a_menu)
	{
		if (!a_menu) {
			return;
		}
		// A virtual call, so a plugin loaded after this one that replaces the
		// function still runs.
		a_menu->CreateModdedInventoryItem();
	}
}
