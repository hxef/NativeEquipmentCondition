#pragma once

// The settings under [Features]: All of NEC, the switch on each row of
// Features.cpp and the choices that go with them. Included by Settings.h
// only, after Live. Private to this folder.
namespace Settings
{
	// Features. Each is the switch on a row of Features.cpp.

	// All of NEC, switched on NEC's main MCM page. While it is off every hook
	// gives the vanilla result, see Core/CallPatch/Sets.cpp, and weapons and
	// armor keep their condition. No feature's row. On by default.
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

	// Weapons and armor workbenches repair, see Workbench::BenchRepairs. When
	// off they only modify, scrap and rename. No feature's row: the mod lock
	// still holds. On by default.
	inline Live<bool> bWorkbenchRepair{ "Features", "bWorkbenchRepair", true };

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

	// REPAIR on the Pip-Boy's inspect screen, see Workbench/Inspect.h: 0 off,
	// 1 with a spare, 2 as at a workbench. Other numbers read as 0, the
	// default.
	inline Live<std::int32_t> iPipboyRepair{ "Features", "iPipboyRepair", 0 };

	// Which gear with UnscrappableObject can still be a spare, see
	// Workbench/Spares.cpp: 0 none, 1 legendaries and 2 all. Any other number
	// reads as 1, the default.
	inline Live<std::int32_t> iUnscrappableSpares{ "Features", "iUnscrappableSpares", 1 };

	// What a spare repair does with the spare's mods, see
	// Workbench/SpareMods.cpp: 0 uses them up, 1 and 2 hand back those the
	// repair does not need, 3, the default, and 4 ask in a box. Any other
	// number reads as 3.
	inline Live<std::int32_t> iKeepSpareMods{ "Features", "iKeepSpareMods", 3 };

	// Whether iKeepSpareMods at a_value asks in a box which of a spare's mods
	// to use up or keep, 3 and 4, and also any number outside 0 to 4, since
	// it reads as 3.
	[[nodiscard]] constexpr bool KeepSpareModsAsks(std::int32_t a_value) { return a_value < 0 || a_value > 2; }

	// What a mod's row shows in the boxes of iKeepSpareMods 3 and 4: 0 what
	// using up the mod adds or keeping it costs, +3.7% or -4.2%, 1 the
	// condition the repair reaches, 64.2%. Any other number reads as 0.
	inline Live<std::int32_t> iSpareModRows{ "Features", "iSpareModRows", 0 };

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

	// How each place shows condition, see Cnd. iWeaponCnd and
	// iQuickContainerCnd also turn their feature's row on, see Features.cpp.

	// The Weapon CND on the HUD, by the ammo counter.
	inline CndStep iWeaponCnd{ "Features", "iWeaponCnd", Cnd::kBar };

	// The Power armor dash CND, whose row goes with iWeaponCnd's part.
	inline CndStep iPowerArmorCnd{ "Features", "iPowerArmorCnd", Cnd::kBar };

	// Condition on the rows of the quick container.
	inline CndStep iQuickContainerCnd{ "Features", "iQuickContainerCnd", Cnd::kBar };

	// The CND row of item cards. At Off the cards stay vanilla.
	inline CndStep iItemCardCnd{ "Features", "iItemCardCnd", Cnd::kPercent };

	// The wear rate beside the condition on the CND row of item cards, read
	// at every render of a card, see UI/Inventory/ItemCard/WearRate.cpp.
	inline Live<bool> bCardWearRate{ "Features", "bCardWearRate", true };

	// A power armor piece's Health on its item card shown as CND, a bar or a
	// percent as iItemCardCnd says, display only, see
	// UI/Inventory/ItemCard/PowerArmor.cpp.
	inline Live<bool> bPowerArmorCardCnd{ "Features", "bPowerArmorCardCnd", false };

	// How a card writes an item's condition: 0 whole numbers, 1 a tenth only
	// when it is not 0, 2 always a tenth. Any other number reads as 2, see
	// Condition::Decimals.
	inline Live<std::int32_t> iCndDecimals{ "Features", "iCndDecimals", 2 };

	// A CND column in a UI replacer's item lists, read at each list's opening,
	// see UI/Inventory/ItemCard/Column.cpp.
	inline Live<bool> bCndColumn{ "Features", "bCndColumn", true };

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
