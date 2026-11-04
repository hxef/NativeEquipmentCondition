#include "Core/BenchRow.h"

namespace BenchRow
{
	const RE::InventoryUserUIInterfaceEntry* Highlighted(RE::ExamineMenu& a_menu)
	{
		if (!a_menu.itemList.IsObject()) {
			return nullptr;
		}

		const auto  row = static_cast<std::int32_t>(a_menu.GetSelectedIndex());
		const auto& rows = a_menu.invInterface.stackedEntries;
		if (row < 0 || static_cast<std::uint32_t>(row) >= rows.size()) {
			return nullptr;
		}
		return &rows[static_cast<std::size_t>(row)];
	}
}
