#pragma once

#include "Core/Plugin.h"

#include "Condition/Materials/Materials.h"
#include "UI/Repair/SelectedItem.h"

#include <cstdint>
#include <vector>

// The supplies every paid workbench repair takes, iRepairSupplies of each, in
// 3 sets: oil and cloth for a gun or a powered melee weapon, adhesive alone
// for every other weapon, and adhesive and cloth for armor and clothing. A
// melee weapon is powered when its copy carries the Automatic flag, such as
// the Ripper, or when one of its mods is built from Circuitry or Nuclear
// Material, such as a Heated Super Sledge. Private to this folder.
//
// From fSuppliesOnlyFrom, 87.5 by default, so from a card reading 87.5%, or
// 88% at CND decimals Off, a repair takes the supplies alone.
// Below it, it also takes the components for the stretch up to the first
// whole percent of that condition, see BillOf in Cost.h. So a 10mm Pistol at
// 90% costs 1 Oil and 1 Cloth, and at 40% the components from 40% to 88% on
// top. A Baseball Bat at
// 90% costs 1 Adhesive, and a Combat Armor Chest Piece at 90% 1 Adhesive and
// 1 Cloth. The crafting perks take nothing off the supplies. MEND, a spare and
// a trader take none.
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

	// fSuppliesOnlyFrom in tenths of the number an item's card shows, 0 to
	// 1000, see Repair::CardTenthsFrom: the condition from which a repair
	// takes only its supplies. 875 by default, or 880 at CND decimals Off.
	[[nodiscard]] std::uint32_t FromTenths();

	// Whether the load order holds at least 1 of the 3 supplies, so at least
	// 1 set has something for Of to take. Workbench::SuppliesAlone asks it.
	[[nodiscard]] bool AnyHeld();

	// Whether a repair of a_item from its card's number takes components:
	// always while the supplies are off or the load order has none of
	// a_item's, else only below FromTenths(). BillOf bills the same way. The
	// repair lock holds back only such a repair, see Lock.h.
	[[nodiscard]] bool TakesComponents(const SelectedItem::Item& a_item);

	// The supplies of a_item, Count() of each, the oil or the adhesive first
	// and the cloth, where the set has it, second. A supply the load order
	// lacks is left out, and so is every supply while Count() is 0.
	[[nodiscard]] std::vector<Materials::Part> Of(const SelectedItem::Item& a_item);

	// a_bill with the supplies of a_item added on top of it. The supplies come
	// first, in the order of Of, then the rest of a_bill in its own order. A
	// component a_bill already holds, such as the Oil of a receiver, is added
	// to the supply's row, so the box lists it once.
	[[nodiscard]] std::vector<Materials::Part> Add(const std::vector<Materials::Part>& a_bill, const SelectedItem::Item& a_item);

	// Writes the supplies' line in NEC.log, as the settings stand at the
	// start.
	void Say();

	// Writes in NEC.log each supply Fallout4.esm has no component for, once
	// every time game data has loaded.
	void SayMissing();
}
