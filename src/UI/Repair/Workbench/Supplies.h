#pragma once

#include "Core/Plugin.h"

#include "Condition/Condition.h"
#include "Condition/Materials/Materials.h"

#include <cstdint>
#include <vector>

// The supplies every paid workbench repair takes: oil and cloth for a weapon,
// adhesive and cloth for armor and clothing, iRepairSupplies of each. Private
// to this folder.
//
// From iSuppliesOnlyFrom up, 80 by default, a repair takes the supplies alone.
// Below it, it also takes the components for the stretch up to that
// condition, see BillOf in Cost.h. So a 10mm Pistol at 85% costs 1 Oil and 1
// Cloth, and at 40% the components from 40% to 80% on top. The crafting perks
// take nothing off the supplies. MEND, a spare and a trader take none.
//
// The 3 components are Fallout4.esm's own, looked up at every bill, so a mod
// that changes them is followed and nothing is kept between loads.
namespace Workbench::Supplies
{
	// The most of each supply a repair takes.
	inline constexpr std::int32_t MOST = 5;

	// iRepairSupplies kept between 0 and MOST. At 0 a repair takes only the
	// components the item is built from, as before supplies.
	[[nodiscard]] std::uint32_t Count();

	// iSuppliesOnlyFrom kept between 0 and 100: the condition from which a
	// repair takes only its supplies.
	[[nodiscard]] std::uint32_t From();

	// Whether a repair of a_kind from a_percent takes components: always while
	// the supplies are off or the load order has none of a_kind's, else only
	// below From(). BillOf bills the same way. The repair lock holds back only
	// such a repair, see Lock.h.
	[[nodiscard]] bool TakesComponents(std::uint32_t a_percent, Condition::Kind a_kind);

	// The supplies of a_kind, Count() of each, the oil or the adhesive first
	// and the cloth second. A supply the load order lacks is left out, and so
	// is every supply while Count() is 0.
	[[nodiscard]] std::vector<Materials::Part> Of(Condition::Kind a_kind);

	// a_bill with the supplies of a_kind added on top of it. The supplies come
	// first, in the order of Of, then the rest of a_bill in its own order. A
	// component a_bill already holds, such as the Oil of a receiver, is added
	// to the supply's row, so the box lists it once.
	[[nodiscard]] std::vector<Materials::Part> Add(const std::vector<Materials::Part>& a_bill, Condition::Kind a_kind);

	// Writes the supplies' line in NEC.log, as the settings stand at the
	// start.
	void Say();

	// Writes in NEC.log each supply Fallout4.esm has no component for, once
	// every time game data has loaded.
	void SayMissing();
}
