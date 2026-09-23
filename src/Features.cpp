#include "Core/Feature.h"

#include "Condition/CraftingPerks/CraftingPerks.h"
#include "Condition/Materials.h"
#include "Condition/Provenance/Provenance.h"
#include "Condition/WeaponWear/WeaponWear.h"
#include "Core/Settings.h"
#include "Gameplay/CritMeter.h"
#include "Gameplay/FireRate.h"
#include "Gameplay/HealthDamage/HealthDamage.h"
#include "Gameplay/ItemValue.h"
#include "Gameplay/Jam.h"
#include "Gameplay/SpawnCondition/SpawnCondition.h"
#include "Gameplay/WeaponEvents/WeaponEvents.h"
#include "UI/Hud/HudCondition.h"
#include "UI/Hud/HudParts.h"
#include "UI/Hud/PowerArmorCondition/PowerArmorCondition.h"
#include "UI/Hud/QuickContainer/QuickContainer.h"
#include "UI/Inventory/ItemCard/ItemCard.h"
#include "UI/Inventory/Pipboy.h"
#include "UI/LoadingTips.h"
#include "UI/Repair/ConfirmScroll.h"
#include "UI/Repair/ConsoleRepair.h"
#include "UI/Repair/VendorRepair/VendorRepair.h"
#include "UI/Repair/Workbench/Workbench.h"

namespace
{
	// Every feature, in the order they run. Install and Load run top to bottom
	// and Unload bottom to top, so a row that reads what a row above kept sits
	// below it. A row with a switch is skipped entirely while it is off.
	constexpr Feature FEATURES[]{
		// Condition: what condition is, and what repairing costs.

		// What a weapon is built from, read off the game's own recipes.
		{ .name = "Materials", .Load = Materials::Load, .Unload = Materials::Unload },
		// Which perk prices a repair. Reads the same recipes as Materials, so
		// it comes after.
		{ .name = "CraftingPerks", .Install = CraftingPerks::Install, .Load = CraftingPerks::Load, .Unload = CraftingPerks::Unload },
		// What an ordinary weapon hits for, which every wear rate is measured
		// against.
		{ .name = "WeaponWear", .Load = WeaponWear::Load },
		// Where a weapon came from, measured from every leveled list and
		// outfit. Only SpawnCondition uses it, so its switch covers both.
		{ .name = "Provenance", .on = &Settings::bSpawnCondition, .Load = Provenance::Load, .Unload = Provenance::Unload },

		// Gameplay: hooks that change play.

		// Wear on every shot and every blow of the player's weapon.
		{ .name = "WeaponEvents", .Install = WeaponEvents::Install, .Load = WeaponEvents::Load },
		// A worn weapon does less damage.
		{ .name = "HealthDamage", .Install = HealthDamage::Install },
		// A worn item is worth less, everywhere the game prints a price.
		{ .name = "ItemValue", .Install = ItemValue::Install },
		// A worn automatic weapon fires slower, in anybody's hands.
		{ .name = "FireRate", .on = &Settings::bFireRate, .Install = FireRate::Install, .Unload = FireRate::Unload },
		// A worn weapon fills the VATS critical meter slower, and an NPC's worn
		// weapon lands fewer critical hits.
		{ .name = "CritMeter", .on = &Settings::bCritMeter, .Install = CritMeter::Install },
		// A worn gun can jam.
		{ .name = "Jam", .on = &Settings::bJam, .Load = Jam::Load, .Unload = Jam::Unload },
		// A weapon spawns at a condition that suits where it came from.
		{ .name = "SpawnCondition", .on = &Settings::bSpawnCondition, .Install = SpawnCondition::Install },

		// UI: what is drawn into the game's own menus.

		// A CND row on every item card, and a worn gun's fire rate wherever a
		// menu shows or weighs it. Asks FireRate whether a worn gun slows, so
		// it comes after.
		{ .name = "ItemCard", .Install = ItemCard::Install, .OnMovieLoaded = ItemCard::OnMovieLoaded },
		// Faded names for worn out weapons in the Pip-Boy's lists.
		{ .name = "Pipboy", .OnMovieLoaded = Pipboy::OnMovieLoaded },
		// What both HUD readouts share, on their switch. Their colour targets
		// go with the HUD menu they were made for.
		{ .name = "HudParts", .on = &Settings::bHudCondition, .Install = HudParts::Install },
		// A CND bar in the HUD's ammo counter.
		{ .name = "HudCondition", .on = &Settings::bHudCondition, .OnMovieLoaded = HudCondition::OnMovieLoaded },
		// The same bar on the power armor dash, on the same switch.
		{ .name = "PowerArmorCondition", .on = &Settings::bHudCondition, .Load = PowerArmorCondition::Load, .OnMovieLoaded = PowerArmorCondition::OnMovieLoaded },
		// Condition meters on the rows of the HUD's quick container.
		{ .name = "QuickContainer", .on = &Settings::bQuickContainer, .Install = QuickContainer::Install, .OnMovieLoaded = QuickContainer::OnMovieLoaded },
		// Repairing a worn weapon at the weapon workbench, for components.
		{ .name = "Workbench", .Install = Workbench::Install, .Load = Workbench::Load, .OnMovieLoaded = Workbench::OnMovieLoaded },
		// A long component list in a workbench's confirmation box shows more
		// rows and scrolls with the mouse wheel and the keys.
		{ .name = "ConfirmScroll", .on = &Settings::bConfirmScroll, .Install = ConfirmScroll::Install },
		// Paying a trader who deals in weapons to repair one, for caps.
		{ .name = "VendorRepair", .on = &Settings::bVendorRepair, .Install = VendorRepair::Install },
		// Setting the weapon in hand to any condition from the console.
		{ .name = "ConsoleRepair", .Install = ConsoleRepair::Install },
		// Loading screen tips about condition.
		{ .name = "LoadingTips", .on = &Settings::bLoadingTips, .Install = LoadingTips::Install, .Load = LoadingTips::Load, .Unload = LoadingTips::Unload },
	};
}

std::span<const Feature> Features()
{
	return FEATURES;
}
