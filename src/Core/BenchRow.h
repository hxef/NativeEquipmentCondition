#pragma once

#include "Core/Plugin.h"

// The row a workbench's item list has highlighted, which both the repair and
// the SCRAP button act on.
namespace BenchRow
{
	// The highlighted row, or null with no list yet or a row out of range.
	// The list is asked first, since the row number comes out of it and the
	// game reads it without checking. Flash calls in during the menu's first
	// moments, before any list exists.
	[[nodiscard]] const RE::InventoryUserUIInterfaceEntry* Highlighted(RE::ExamineMenu& a_menu);
}
