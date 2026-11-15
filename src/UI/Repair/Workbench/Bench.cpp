#include "UI/Repair/Workbench/Bench.h"

#include "Core/BenchRow.h"
#include "Core/Settings.h"
#include "UI/Repair/Workbench/Supplies.h"

#include <algorithm>

namespace Workbench
{
	std::uint32_t FreeFrom()
	{
		constexpr auto full = static_cast<std::int32_t>(Repair::FULL);
		return static_cast<std::uint32_t>(std::clamp(Settings::iFreeMendFrom.GetValue(), 0, full));
	}

	bool SuppliesAlone()
	{
		return Supplies::Count() > 0 && !FreeRepairs() && Supplies::From() < FreeFrom() && Supplies::AnyHeld();
	}

	std::uint32_t SuppliesOnlyFrom()
	{
		return SuppliesAlone() ? Supplies::From() : FreeFrom();
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
