#include "UI/Repair/Workbench/Bench.h"

namespace Workbench
{
	Selection Selected(RE::ExamineMenu* a_menu)
	{
		// The item list has to be asked first, since the row number comes out
		// of it and the game reads it without checking. Flash calls in during
		// the menu's first moments, before any list exists.
		if (!a_menu || a_menu->inspectMode || !a_menu->itemList.IsObject()) {
			return {};
		}

		const auto  row = static_cast<std::int32_t>(a_menu->GetSelectedIndex());
		const auto& rows = a_menu->invInterface.stackedEntries;
		if (row < 0 || static_cast<std::uint32_t>(row) >= rows.size()) {
			return {};
		}
		return Selection{ SelectedItem::Read(rows[static_cast<std::size_t>(row)]) };
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

	bool Repairing(RE::ExamineMenu* a_menu)
	{
		return a_menu && a_menu->repairing;
	}
}
