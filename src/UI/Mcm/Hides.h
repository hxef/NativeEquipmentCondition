#pragma once

#include "Condition/Condition.h"
#include "Core/Settings/Settings.h"
#include "UI/Inventory/ItemCard/ItemCard.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <string_view>

// The hidden switches that hide rows of NEC's pages while a setting makes
// them moot. Private to this folder.
namespace Mcm
{
	// A hidden switch that hides rows while a number is at its off value, as a
	// switch that is off hides the rows under it, or while a few settings are.
	// Each has config.json's refreshMenu, so the rows hide and come back as it
	// moves. rows are the settings of the rows it hides, which their block's
	// note leaves out too, and nullptr after the last. A row on 2 hides shows
	// only while both do, as config.json joins their numbers with AND.
	struct Hide
	{
		std::string_view                      id;
		bool                                  (*shows)();
		std::array<const Settings::Named*, 8> rows;
	};

	// Whether anything repairs as a workbench does: the bench itself, or
	// REPAIR on the Pip-Boy's inspect screen, iPipboyRepair 1 or 2 as set.
	// When neither does, only traders repair.
	[[nodiscard]] inline bool BenchWayRepairs()
	{
		const auto pipboy = Settings::iPipboyRepair.GetValue();
		return Settings::bWorkbenchRepair.GetValue() || pipboy == 1 || pipboy == 2;
	}

	// The 6 CND steppers, 1 for each place that shows an item's condition.
	inline constexpr std::array CND_STEPS{ &Settings::iWeaponCnd, &Settings::iPowerArmorCnd, &Settings::iPickupCnd,
		&Settings::iQuickContainerCnd, &Settings::iApparelFigureCnd, &Settings::iItemCardCnd };

	// Whether any place shows condition as a_form.
	[[nodiscard]] inline bool AnyCndAt(Settings::Cnd a_form)
	{
		return std::ranges::any_of(CND_STEPS, [&](const Settings::CndStep* a_step) { return a_step->Form() == a_form; });
	}

	// Whether either penalty point marks a bar, from 0.01 to 0.99.
	[[nodiscard]] inline bool AnyPenaltyPoint()
	{
		return Condition::StartPercent(Condition::Kind::kWeapon) != 0 || Condition::StartPercent(Condition::Kind::kArmor) != 0;
	}

	// Whether a place that can draw the penalty mark shows a bar, the item
	// card's counted only with a_card. The quick container's meter never
	// draws it, the Pickup CND bar only while Pickup CND penalty mark is on,
	// and the item card's bar always draws its own mark, a gap through the
	// fill or a faint line past it.
	[[nodiscard]] inline bool MarkedBarShows(bool a_card = true)
	{
		return std::ranges::any_of(CND_STEPS, [&](const Settings::CndStep* a_step) {
			return a_step != &Settings::iQuickContainerCnd && (a_card || a_step != &Settings::iItemCardCnd) &&
			       (a_step != &Settings::iPickupCnd || Settings::bPickupPenaltyMark.GetValue()) &&
			       a_step->Form() == Settings::Cnd::kBar;
		});
	}

	// Jams start at shows while the jam chance is above 0, Perk lock and Lock
	// spares too while Lock repairs is not Off, and Supplies only from while
	// Servicing supplies is above 0. An iRepairLock of 3 or -1 locks as By
	// its mods does, so only 0 hides them. The 8 rows that tune paid repairs
	// show while Workbench repair cost is above 0, since at 0 every repair is
	// a free MEND and no repair box opens, see FreeRepairs in
	// UI/Repair/Workbench/Bench.h, and while something repairs as a workbench
	// does. The Maintenance header hides with them, as no row is left under
	// it. Workbench repair cost and the Repair with spares section show only
	// while something repairs so too, and Repair box opens on only while a
	// levels box can open, at the bench or as Workbench in the Pip-Boy. Lock
	// spares too also hides while Spare repair is off, by its config.json
	// row, and so do Keep spare mods and Spare mod rows. Spare mod rows shows
	// only while Keep spare mods opens a box, at Choose mods to use up or
	// to keep. The Penalty mark section shows while either penalty point
	// marks a bar, from 0.01 to 0.99, and a place that draws the mark shows a
	// bar, the Pickup CND bar only while Pickup CND penalty mark is on.
	// Pickup CND penalty mark shows while Pickup CND is at Bar and either
	// penalty point is from 0.01 to 0.99, and refreshes the menu, since the
	// Penalty mark section counts it. Penalty mark as a line also needs a bar
	// other than the item card's, since the card's bar always draws its own
	// mark. Its 2 points sit on the Weapons and the Armor page, so the
	// section's id names the first, and both sliders refresh the menu.
	// At Off each CND stepper hides the rows of its place, and at any form
	// but Bar the rows that size a bar, flip it or show its word. Weapon CND
	// percent in 1 line shows only while Weapon CND is at Percent. A part left
	// to another mod leaves its stepper at Off in memory, see Gate::SetOff, so
	// its Bar rows hide too and a block's note leaves them out. The 3 item
	// card switches hide while Item card CND is Off, and CND decimals while no
	// place shows a percent and no repair box can open, at a workbench, in
	// the Pip-Boy or at a trader. CND column in UI replacer lists shows only
	// after a list with columns opened this session, see
	// ItemCard::ColumnOffered, and while it is on there, the column's number
	// keeps CND decimals shown.
	inline constexpr Hide HIDES[]{
		{ "fJamChance:Jams"sv, [] { return Settings::fJamChance.GetValue() > 0.0F; }, { &Settings::fJamStart } },
		{ "iRepairLock:Lock"sv, [] { return Settings::iRepairLock.GetValue() != 0; }, { &Settings::iPerkLock, &Settings::bLockSpares } },
		{ "iRepairSupplies:Supplies"sv, [] { return Settings::iRepairSupplies.GetValue() > 0; }, { &Settings::fSuppliesOnlyFrom } },
		{ "bWorkbenchRepair:Levels"sv, [] { return Settings::bWorkbenchRepair.GetValue() || Settings::iPipboyRepair.GetValue() == 2; },
			{ &Settings::iRepairBoxRow } },
		{ "bWorkbenchRepair:Repairs"sv, BenchWayRepairs,
			{ &Settings::fBenchCostMult, &Settings::bSpareRepair, &Settings::fSpareWorthMult, &Settings::iUnscrappableSpares,
				&Settings::iKeepSpareMods, &Settings::iSpareModRows } },
		{ "iKeepSpareMods:Choose"sv, [] { return Settings::KeepSpareModsAsks(Settings::iKeepSpareMods.GetValue()); },
			{ &Settings::iSpareModRows } },
		{ "fBenchCostMult:Paid"sv, [] { return Settings::fBenchCostMult.GetValue() > 0.0F && BenchWayRepairs(); },
			{ &Settings::fFreeMendFrom, &Settings::iRepairSupplies, &Settings::fSuppliesOnlyFrom, &Settings::iRepairLock,
				&Settings::iPerkLock, &Settings::iPerkComponentDiscount, &Settings::bLockSpares, &Settings::iRepairBoxRow } },
		{ "fWeaponPenaltyStart:Marks"sv, [] { return AnyPenaltyPoint() && MarkedBarShows(); },
			{ &Settings::fPenaltyMarkWidth, &Settings::bPenaltyMarkAsLine } },
		{ "bPenaltyMarkAsLine:Notches"sv, [] { return MarkedBarShows(false); }, { &Settings::bPenaltyMarkAsLine } },
		{ "iWeaponCnd:Shown"sv, [] { return Settings::iWeaponCnd.Form() != Settings::Cnd::kOff; },
			{ &Settings::fHudBarX, &Settings::fHudBarY, &Settings::bHudBarHidesWithCounter, &Settings::bHudBarStandsAlone } },
		{ "iWeaponCnd:Bar"sv, [] { return Settings::iWeaponCnd.Form() == Settings::Cnd::kBar; },
			{ &Settings::fHudBarLength, &Settings::fHudBarThickness, &Settings::bHudBarShowsWord, &Settings::bHudBarFlipped } },
		{ "iWeaponCnd:Percent"sv, [] { return Settings::iWeaponCnd.Form() == Settings::Cnd::kPercent; }, { &Settings::bHudBarPercentOneLine } },
		{ "iPowerArmorCnd:Shown"sv, [] { return Settings::iPowerArmorCnd.Form() != Settings::Cnd::kOff; },
			{ &Settings::fPowerArmorBarX, &Settings::fPowerArmorBarY } },
		{ "iPowerArmorCnd:Bar"sv, [] { return Settings::iPowerArmorCnd.Form() == Settings::Cnd::kBar; },
			{ &Settings::fPowerArmorBarLength, &Settings::fPowerArmorBarThickness, &Settings::bPowerArmorBarShowsWord,
				&Settings::bPowerArmorBarFlipped } },
		{ "iPickupCnd:Shown"sv, [] { return Settings::iPickupCnd.Form() != Settings::Cnd::kOff; },
			{ &Settings::fPickupBarX, &Settings::fPickupBarY } },
		{ "iPickupCnd:Bar"sv, [] { return Settings::iPickupCnd.Form() == Settings::Cnd::kBar; },
			{ &Settings::fPickupBarLength, &Settings::fPickupBarThickness, &Settings::bPickupBarShowsWord, &Settings::bPickupBarFlipped } },
		{ "bPickupPenaltyMark:Marks"sv, [] { return Settings::iPickupCnd.Form() == Settings::Cnd::kBar && AnyPenaltyPoint(); },
			{ &Settings::bPickupPenaltyMark } },
		{ "iItemCardCnd:Shown"sv, [] { return Settings::iItemCardCnd.Form() != Settings::Cnd::kOff; },
			{ &Settings::bCardWearRate, &Settings::bPowerArmorCardCnd, &Settings::bCndColumn } },
		{ "iQuickContainerCnd:Shown"sv, [] { return Settings::iQuickContainerCnd.Form() != Settings::Cnd::kOff; },
			{ &Settings::bQuickContainerCndAtRight } },
		{ "iQuickContainerCnd:Bar"sv, [] { return Settings::iQuickContainerCnd.Form() == Settings::Cnd::kBar; },
			{ &Settings::bQuickContainerBarFlipped } },
		{ "iItemCardCnd:Bar"sv, [] { return Settings::iItemCardCnd.Form() == Settings::Cnd::kBar; },
			{ &Settings::fItemCardBarThickness, &Settings::bItemCardBarFlipped } },
		{ "iApparelFigureCnd:Bar"sv, [] { return Settings::iApparelFigureCnd.Form() == Settings::Cnd::kBar; },
			{ &Settings::fPipboyBarThickness, &Settings::bPipboyBarFlipped } },
		{ "iCndDecimals:Numbers"sv,
			[] {
				return AnyCndAt(Settings::Cnd::kPercent) || BenchWayRepairs() || Settings::bVendorRepair.GetValue() ||
				       (Settings::iItemCardCnd.Form() != Settings::Cnd::kOff && Settings::bCndColumn.GetValue() && ItemCard::ColumnOffered());
			},
			{ &Settings::iCndDecimals } },
		{ "bCndColumn:Offered"sv, [] { return ItemCard::ColumnOffered(); }, { &Settings::bCndColumn } },
	};

	// The hide a_id names, or nullptr.
	[[nodiscard]] constexpr const Hide* HideOf(std::string_view a_id)
	{
		const auto it = std::ranges::find(HIDES, a_id, &Hide::id);
		return it != std::end(HIDES) ? &*it : nullptr;
	}
}
