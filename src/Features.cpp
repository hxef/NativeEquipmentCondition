#include "Core/Feature.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/CraftingPerks/CraftingPerks.h"
#include "Condition/Materials/Materials.h"
#include "Condition/Provenance/Provenance.h"
#include "Condition/WeaponWear/WeaponWear.h"
#include "Core/Settings.h"
#include "Gameplay/ArmorEvents.h"
#include "Gameplay/ArmorRating.h"
#include "Gameplay/BrokenEquip.h"
#include "Gameplay/CritMeter.h"
#include "Gameplay/FireRate/FireRate.h"
#include "Gameplay/HealthDamage/HealthDamage.h"
#include "Gameplay/ItemValue.h"
#include "Gameplay/Jam.h"
#include "Gameplay/SpawnCondition/SpawnCondition.h"
#include "Gameplay/WeaponEvents/WeaponEvents.h"
#include "UI/Hud/HudCondition.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Hud/PowerArmorCondition/PowerArmorCondition.h"
#include "UI/Hud/QuickContainer/QuickContainer.h"
#include "UI/InspectPrice.h"
#include "UI/Inventory/ItemCard/ItemCard.h"
#include "UI/Inventory/PaperDoll.h"
#include "UI/Inventory/Pipboy.h"
#include "UI/LoadingTips.h"
#include "UI/Mcm/Mcm.h"
#include "UI/Repair/ConfirmScroll/ConfirmScroll.h"
#include "UI/Repair/ConsoleRepair.h"
#include "UI/Repair/VendorRepair/VendorRepair.h"
#include "UI/Repair/Workbench/Workbench.h"

#include <cstddef>
#include <iterator>

namespace
{
	// -------------------------------------------------------------------
	// The features
	// -------------------------------------------------------------------

	// Every feature, in the order they run. Install and Load run top to bottom
	// and Unload bottom to top, so a row that reads what a row above kept sits
	// below it.
	constexpr Feature FEATURES[]{
		// Condition: what condition is, and what repairing costs.

		// Which pieces of armor take a mod that adds protection, an effect or
		// a bonus. Which armor wears rests on it, and Materials and
		// CraftingPerks ask that as they load, so it comes first.
		{ .name = "ArmorMods", .Load = ArmorWear::LoadMods, .Unload = ArmorWear::UnloadMods },
		// What a weapon or a piece of armor is built from, read off the game's
		// own recipes.
		{ .name = "Materials", .Load = Materials::Load, .Unload = Materials::Unload },
		// Which perk prices a repair. Reads the same recipes as Materials, so
		// it comes after.
		{ .name = "CraftingPerks", .part = Part::kPerks, .Install = CraftingPerks::Install, .Load = CraftingPerks::Load, .Unload = CraftingPerks::Unload },
		// What an ordinary weapon hits for, which every wear rate is measured
		// against.
		{ .name = "WeaponWear", .Load = WeaponWear::Load },
		// How armor wears, measured against the same ordinary weapon, so it
		// comes after.
		{ .name = "ArmorWear", .Load = ArmorWear::Load },
		// Where a weapon or a piece of armor came from, measured from every
		// leveled list and outfit. Only SpawnCondition uses it, and it measures
		// whatever the switch says.
		{ .name = "Provenance", .Load = Provenance::Load, .Unload = Provenance::Unload },

		// Gameplay: hooks that change play.

		// Wear on every shot and every blow of the player's weapon.
		{ .name = "WeaponEvents", .part = Part::kGunWear, .Install = WeaponEvents::Install, .Load = WeaponEvents::Load },
		// Wear on the armor a blow lands on, whoever wears it.
		{ .name = "ArmorEvents", .Load = ArmorEvents::Load },
		// A worn weapon does less damage.
		{ .name = "HealthDamage", .part = Part::kDamage, .Install = HealthDamage::Install },
		// A worn piece of armor protects less.
		{ .name = "ArmorRating", .part = Part::kArmor, .Install = ArmorRating::Install },
		// A broken item comes off when asked and goes on only once repaired.
		{ .name = "BrokenEquip", .part = Part::kBroken, .Install = BrokenEquip::Install },
		// A worn item is worth less, everywhere the game prints a price.
		{ .name = "ItemValue", .part = Part::kPrices, .Install = ItemValue::Install },
		// A worn automatic weapon fires slower, in anybody's hands.
		{ .name = "FireRate", .on = &Settings::bFireRate, .part = Part::kFireRate, .Install = FireRate::Install, .Unload = FireRate::Unload },
		// A worn weapon fills the VATS critical meter slower, and an NPC's worn
		// weapon lands fewer critical hits.
		{ .name = "CritMeter", .on = &Settings::bCritMeter, .part = Part::kCritMeter, .Install = CritMeter::Install },
		// A worn gun can jam. Every shot's roll comes through WeaponEvents'
		// fire call, so it rides on Gun wear from firing.
		{ .name = "Jam", .on = &Settings::bJam, .part = Part::kJam, .needs = Part::kGunWear, .Install = Jam::Install, .Load = Jam::Load, .Unload = Jam::Unload },
		// A weapon or a piece of armor spawns at a condition that suits where
		// it came from.
		{ .name = "SpawnCondition", .on = &Settings::bSpawnCondition, .part = Part::kSpawn, .Install = SpawnCondition::Install },

		// UI: what is drawn into the game's own menus.

		// A CND row on every item card, and a worn gun's fire rate wherever a
		// menu shows or weighs it. Asks FireRate whether a worn gun slows, so
		// it comes after.
		{ .name = "ItemCard", .part = Part::kCardCnd, .Install = ItemCard::Install, .OnMovieLoaded = ItemCard::OnMovieLoaded },
		// Faded names for worn out items in the Pip-Boy's lists.
		{ .name = "Pipboy", .OnMovieLoaded = Pipboy::OnMovieLoaded },
		// A CND bar over each body region of the Pip-Boy's paper doll.
		{ .name = "PaperDoll", .OnMovieLoaded = PaperDoll::OnMovieLoaded },
		// What both HUD readouts share, on their switch. Their colour targets
		// go with the HUD menu they were made for.
		{ .name = "HudParts", .on = &Settings::bHudCondition, .part = Part::kHudBar, .Install = HudParts::Install, .Load = HudParts::Load },
		// A CND bar in the HUD's ammo counter.
		{ .name = "HudCondition", .on = &Settings::bHudCondition, .OnMovieLoaded = HudCondition::OnMovieLoaded },
		// The same bar on the power armor dash, on the same switch.
		{ .name = "PowerArmorCondition", .on = &Settings::bHudCondition, .Load = PowerArmorCondition::Load, .OnMovieLoaded = PowerArmorCondition::OnMovieLoaded },
		// Condition meters on the rows of the HUD's quick container.
		{ .name = "QuickContainer", .on = &Settings::bQuickContainer, .part = Part::kQuick, .Install = QuickContainer::Install, .OnMovieLoaded = QuickContainer::OnMovieLoaded },
		// Repairing a worn weapon or piece of armor at its workbench, for
		// components.
		{ .name = "Workbench", .part = Part::kBench, .Install = Workbench::Install, .Load = Workbench::Load, .OnMovieLoaded = Workbench::OnMovieLoaded },
		// A long component list in a workbench's confirmation box shows more
		// rows and scrolls with the mouse wheel and the keys.
		{ .name = "ConfirmScroll", .on = &Settings::bConfirmScroll, .part = Part::kScroll, .Install = ConfirmScroll::Install },
		// Paying a trader who deals in weapons, armor or clothing to repair
		// one, for caps.
		{ .name = "VendorRepair", .on = &Settings::bVendorRepair, .part = Part::kTraderRepairs, .Install = VendorRepair::Install },
		// The inspect screen at a trader shows the price for the side the
		// item is on, where the game shows whichever side it priced last.
		{ .name = "InspectPrice", .on = &Settings::bInspectPrice, .part = Part::kInspect, .Install = InspectPrice::Install },
		// Setting the weapon in hand or the armor worn to any condition from
		// the console.
		{ .name = "ConsoleRepair", .part = Part::kSrm, .Install = ConsoleRepair::Install },
		// Loading screen tips about condition.
		{ .name = "LoadingTips", .on = &Settings::bLoadingTips, .part = Part::kTips, .Install = LoadingTips::Install, .Load = LoadingTips::Load, .Unload = LoadingTips::Unload },
		// The settings page in the Mod Configuration Menu, where it is
		// installed.
		{ .name = "Mcm", .OnMovieLoaded = Mcm::OnMovieLoaded },
	};

	// -------------------------------------------------------------------
	// Checks on the table, while it compiles
	// -------------------------------------------------------------------

	// Every row that patches has a part of its own, so a place that names no
	// part has one, and a part's loss reads the same wherever it is asked.
	consteval bool PartsOfRows()
	{
		for (std::size_t i = 0; i < std::size(FEATURES); i++) {
			const auto part = FEATURES[i].part;
			if ((FEATURES[i].Install && part == Part::kNone) || part >= Part::kTrace) {
				return false;
			}
			for (std::size_t j = 0; j < i; j++) {
				if (part != Part::kNone && FEATURES[j].part == part) {
					return false;
				}
			}
		}
		return true;
	}
	static_assert(PartsOfRows());

	// Every .needs is the part of a row above it, so its places are settled
	// before the turn of the row that rides on it.
	consteval bool NeedsRowsAbove()
	{
		for (std::size_t i = 0; i < std::size(FEATURES); i++) {
			if (FEATURES[i].needs == Part::kNone) {
				continue;
			}
			bool found = false;
			for (std::size_t j = 0; j < i; j++) {
				found = found || FEATURES[j].part == FEATURES[i].needs;
			}
			if (!found) {
				return false;
			}
		}
		return true;
	}
	static_assert(NeedsRowsAbove());

	// Of the rows on 1 switch, exactly 1 has a part, the one PartOf gives.
	consteval bool SwitchesNamed()
	{
		for (const auto& row : FEATURES) {
			if (!row.on) {
				continue;
			}
			std::size_t named = 0;
			for (const auto& other : FEATURES) {
				if (other.on == row.on && other.part != Part::kNone) {
					named++;
				}
			}
			if (named != 1) {
				return false;
			}
		}
		return true;
	}
	static_assert(SwitchesNamed());
}

std::span<const Feature> Features()
{
	return FEATURES;
}

Part PartOf(const Settings::Named& a_switch)
{
	for (const auto& row : FEATURES) {
		if (row.on && row.part != Part::kNone && static_cast<const Settings::Named*>(row.on) == &a_switch) {
			return row.part;
		}
	}
	return Part::kNone;
}
