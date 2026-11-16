#pragma once

// The settings under [Features]: All of NEC, the switch on each row of
// Features.cpp and the choices that go with them. Included by Settings.h
// only, after Live.
namespace Settings
{
	// Features. Each is the switch on a row of Features.cpp.

	// All of NEC, switched on NEC's main MCM page. While it is off every hook
	// gives the vanilla result, see Core/CallPatch/Sets.cpp, and every item
	// keeps its condition. No feature's row. On by default.
	inline Live<bool> bNecOn{ "Features", "bNecOn", true };

	// What code outside a hook asks before it changes or shows anything.
	[[nodiscard]] inline bool NecOn() { return bNecOn.GetValue(); }

	// Apparel that gives nothing, see ArmorWear::GivesAnything, never wears
	// and shows no condition. No feature's row: Condition::WhyNoCondition
	// reads it for every item it is asked about. Off by default.
	inline Live<bool> bPlainApparelNeverWears{ "Features", "bPlainApparelNeverWears", false };

	// Weapons and armor spawn already worn, see SpawnCondition.h. Provenance
	// measures the load order for it at every data load, whatever the switch
	// says, so it works as soon as it is switched on in play.
	inline Live<bool> bSpawnCondition{ "Features", "bSpawnCondition", true };

	// Traders who deal in weapons, armor or clothing repair them for caps.
	inline Live<bool> bVendorRepair{ "Features", "bVendorRepair", true };

	// A sign on the barter screen says how far the trader repairs each kind,
	// see VendorRepair/Sign.h. No feature's row: it rides on Trader repairs.
	// On by default.
	inline Live<bool> bTraderSign{ "Features", "bTraderSign", true };

	// A USE A SPARE row in a workbench's repair box when the player carries
	// another of the same weapon or armor, with any mods, see
	// Workbench/Spares.h. No feature's row: it rides on Workbench repairs. On
	// by default.
	inline Live<bool> bSpareRepair{ "Features", "bSpareRepair", true };

	// Which gear with UnscrappableObject can still be a spare, see
	// Workbench/Spares.cpp: 0 none, 1 legendaries and 2 all. Any other number
	// reads as 1, the default.
	inline Live<std::int32_t> iUnscrappableSpares{ "Features", "iUnscrappableSpares", 1 };

	// The row a workbench's repair box highlights as it opens, see
	// Workbench/Job.cpp: 0 none, 1 USE A SPARE, 2 the highest level and 3 the
	// lowest. Any other number reads as 2, the default.
	inline Live<std::int32_t> iRepairBoxRow{ "Features", "iRepairBoxRow", 2 };

	// The repair lock, see Workbench/Lock.h: 0 off, 1 every item asks the last
	// rank of its perk, 2 the rank its mods ask. Any other number reads as 2.
	inline Live<std::int32_t> iRepairLock{ "Features", "iRepairLock", 2 };

	// Whether the repair lock holds a spare repair back too, see
	// Workbench/Lock.h. Off by default, so a spare repairs whatever the lock
	// says.
	inline Live<bool> bLockSpares{ "Features", "bLockSpares", false };

	// The rows of the repair boxes at a workbench and at a trader in columns,
	// see Repair/Columns/Columns.h. When off, each row is 1 centred line, as
	// vanilla draws its boxes. No feature's row: it rides on Workbench repairs
	// and Trader repairs. On by default.
	inline Live<bool> bRepairBoxColumns{ "Features", "bRepairBoxColumns", true };

	// What each repair adds to the item's damage or resistances, at the end of
	// the rows of the repair boxes at a workbench and at a trader and of the box
	// that lists the spares, see Repair/Gains.h. When off, Gains::Read reads
	// nothing. No feature's row: it rides on Workbench repairs and Trader
	// repairs. On by default.
	inline Live<bool> bRepairBoxGains{ "Features", "bRepairBoxGains", true };

	// Loading screen tips about condition.
	inline Live<bool> bLoadingTips{ "Features", "bLoadingTips", true };

	// The Weapon CND bar on the HUD, by the ammo counter.
	inline Live<bool> bHudCondition{ "Features", "bHudCondition", true };

	// The Power armor CND bar on the HUD, on the power armor dash. Its row
	// goes with bHudCondition's part, see Features.cpp.
	inline Live<bool> bPowerArmorBar{ "Features", "bPowerArmorBar", true };

	// Condition meters on the rows of the quick container.
	inline Live<bool> bQuickContainer{ "Features", "bQuickContainer", true };

	// The wear rate beside the condition on the CND row of item cards, read
	// at every render of a card, see UI/Inventory/ItemCard/WearRate.cpp.
	inline Live<bool> bCardWearRate{ "Features", "bCardWearRate", true };

	// A power armor piece's Health on its item card shown as CND in percent,
	// display only, see UI/Inventory/ItemCard/PowerArmor.cpp.
	inline Live<bool> bPowerArmorCardCnd{ "Features", "bPowerArmorCardCnd", false };

	// A workbench's confirmation box shows more of a long list and scrolls it,
	// see ConfirmScroll.h.
	inline Live<bool> bConfirmScroll{ "Features", "bConfirmScroll", true };

	// The inspect screen at a trader shows the price for the side the item
	// is on, see InspectPrice.h.
	inline Live<bool> bInspectPrice{ "Features", "bInspectPrice", true };

	// Food the player or a companion carried, and food on the dead, spoils
	// with time, see FoodSpoil.h. Off by default.
	inline Live<bool> bFoodSpoils{ "Features", "bFoodSpoils", false };

	// The fridge icon, see FoodSpoil::FridgeIconOn. On by default.
	inline Live<bool> bFridgeIcon{ "Features", "bFridgeIcon", true };
}
