#pragma once

// The settings under [Balance] and [HUD]: the numbers that tune each feature,
// and where the CND bars sit and when they show. Included by Settings.h only,
// after Live and the settings NEC.ini has before them.
namespace Settings
{
	// Balance. Each tunes a feature and is read where it is used. A floor at
	// 1, or a jam chance of 0, leaves the result vanilla, so such a feature
	// has no switch.

	// What the wear rate of the player's weapons is multiplied by, see
	// WeaponWear/Rate.cpp.
	inline Live<float> fWearRateMult{ "Balance", "fWearRateMult", 1.0F };

	// The same for every NPC's weapons, in place of fWearRateMult.
	inline Live<float> fNpcWearRateMult{ "Balance", "fNpcWearRateMult", 0.33F };

	// What the wear an automatic weapon takes as it fires or cuts is
	// multiplied by, on top of either of those, see WeaponWear/Rate.cpp.
	inline Live<float> fAutoWearRateMult{ "Balance", "fAutoWearRateMult", 1.0F };

	// What the wear rate of the player's armor is multiplied by, see
	// ArmorWear/Rate.cpp.
	inline Live<float> fArmorWearRateMult{ "Balance", "fArmorWearRateMult", 1.0F };

	// The same for every NPC's armor, in place of fArmorWearRateMult.
	inline Live<float> fNpcArmorWearRateMult{ "Balance", "fNpcArmorWearRateMult", 1.0F };

	// What a companion's weapon wear is multiplied by, up to 1, on top of
	// fNpcWearRateMult, see Condition/Crew.h.
	inline Live<float> fCompanionWearRateMult{ "Balance", "fCompanionWearRateMult", 1.0F };

	// The same for the weapons of a settler of a settlement the player owns.
	inline Live<float> fSettlerWearRateMult{ "Balance", "fSettlerWearRateMult", 1.0F };

	// What a companion's armor and clothing wear is multiplied by, up to 1, on
	// top of fNpcArmorWearRateMult.
	inline Live<float> fCompanionArmorWearRateMult{ "Balance", "fCompanionArmorWearRateMult", 1.0F };

	// The same for a settler's armor and clothing.
	inline Live<float> fSettlerArmorWearRateMult{ "Balance", "fSettlerArmorWearRateMult", 1.0F };

	// What a weapon at nothing still hits for, as a share of its full damage,
	// see HealthDamage/Curve.h.
	inline Live<float> fDamageFloor{ "Balance", "fDamageFloor", 0.66F };

	// The condition where wear starts to cost a weapon its damage, fire rate,
	// criticals, accuracy and range, see Condition::Share.
	inline Live<float> fWeaponPenaltyStart{ "Balance", "fWeaponPenaltyStart", 1.0F };

	// What a piece of armor at nothing still protects for, as a share of its
	// full resistances, see ArmorRating.h.
	inline Live<float> fArmorFloor{ "Balance", "fArmorFloor", 0.66F };

	// The condition where wear starts to cost a piece of armor its
	// protection, see Condition::Share.
	inline Live<float> fArmorPenaltyStart{ "Balance", "fArmorPenaltyStart", 1.0F };

	// What a worn item is worth, as its condition raised to this power, see
	// ItemValue.cpp.
	inline Live<float> fValueExponent{ "Balance", "fValueExponent", 1.5F };

	// How fast an automatic weapon at nothing fires, as a share of its own
	// rate, see FireRate.cpp.
	inline Live<float> fFireRateFloor{ "Balance", "fFireRateFloor", 0.75F };

	// How much of the VATS critical meter a shot from a weapon at nothing
	// fills, and how much of its critical chance an NPC's weapon at nothing
	// keeps, see CritMeter.cpp.
	inline Live<float> fCritMeterFloor{ "Balance", "fCritMeterFloor", 0.5F };

	// How accurate a gun at nothing still is, as a share of its accuracy, see
	// Gameplay/Aim/Accuracy.cpp.
	inline Live<float> fAccuracyFloor{ "Balance", "fAccuracyFloor", 0.75F };

	// How far a gun at nothing still carries its damage, as a share of its
	// range, see Gameplay/Aim/Range.cpp.
	inline Live<float> fRangeFloor{ "Balance", "fRangeFloor", 0.75F };

	// How often the player's gun at nothing jams, as a chance per magazine,
	// see Jam.h. Counts as 0 to 1, and 0 turns jams off.
	inline Live<float> fJamChance{ "Balance", "fJamChance", 1.0F };

	// The condition below which the player's guns can jam, see Jam.h. Counts
	// as 0 to 1, and 0 turns jams off.
	inline Live<float> fJamStart{ "Balance", "fJamStart", 0.5F };

	// What a repair at a workbench costs, against the mod as tuned, see
	// Workbench/Cost.h.
	inline Live<float> fBenchCostMult{ "Balance", "fBenchCostMult", 1.0F };

	// What a spare is worth, against what vanilla scrapping gives it at 0.5,
	// see Workbench/Spares.h. Counts as 0.5 to 2.
	inline Live<float> fSpareWorthMult{ "Balance", "fSpareWorthMult", 1.0F };

	// The condition above which a workbench mends an item for nothing, a
	// whole percent, see Workbench/Bench.h.
	inline Live<std::int32_t> iFreeMendAbove{ "Balance", "iFreeMendAbove", 95 };

	// The condition below which a workbench opens no mod slots, a share of
	// full, see Workbench/Bench.h.
	inline Live<float> fModLockStart{ "Balance", "fModLockStart", 1.0F };

	// What a trader asks for a repair, against the mod as tuned, see
	// VendorRepair/Quote.h.
	inline Live<float> fTraderPriceMult{ "Balance", "fTraderPriceMult", 1.0F };

	// How many game days raw food and crops keep before they spoil, see
	// FoodSpoil.h. Counts as 0.25 to 30.
	inline Live<float> fRawFoodDays{ "Balance", "fRawFoodDays", 2.0F };

	// The same for cooked food.
	inline Live<float> fCookedFoodDays{ "Balance", "fCookedFoodDays", 7.0F };

	// How fast food spoils in a fridge or a cooler, 1 for as fast as
	// anywhere. Counts as 0.01 to 1.
	inline Live<float> fFridgeSpoilSpeed{ "Balance", "fFridgeSpoilSpeed", 0.5F };

	// How much spoiled food a chemistry station takes for 1 Fertilizer, see
	// Fertilizer.cpp. Counts as 0 to 20, and 0 removes the Fertilizer row.
	inline Live<std::int32_t> iSpoiledPerFertilizer{ "Balance", "iSpoiledPerFertilizer", 10 };

	// The healing and bonuses spoiled food gives outside Survival, in % of the
	// fresh food's, see FoodSpoil/Eat.cpp. Counts as 0 to 100. In Survival it
	// gives none.
	inline Live<std::int32_t> iSpoiledHealing{ "Balance", "iSpoiledHealing", 10 };

	// The hunger and thirst spoiled food fills in Survival, in % of the fresh
	// food's, see FoodSpoil/Hunger.cpp. Counts as 0 to 100.
	inline Live<std::int32_t> iSpoiledHunger{ "Balance", "iSpoiledHunger", 10 };

	// How many extra rads spoiled food gives, see FoodSpoil/Rads.cpp. Counts
	// as 0 to 5, and 1 gives 15 for raw food and 5 for the rest.
	inline Live<float> fSpoiledRadsMult{ "Balance", "fSpoiledRadsMult", 1.0F };

	// How many times the fresh food's chance of making the player sick
	// spoiled food has in Survival, see FoodSpoil/Sickness.cpp. Counts as 1
	// to 20. Food with no chance of its own takes the smallest any food has.
	inline Live<std::int32_t> iSpoiledSicknessMult{ "Balance", "iSpoiledSicknessMult", 5 };

	// HUD. Where the CND bars sit, how big they are drawn and when the one by
	// the ammo counter hides. Read every frame, so a change shows at once. The
	// 6 place numbers move a bar from its own place in units of the HUD, which
	// is 1280 across and 720 down on any screen, and 0 and 0 is its own place.
	// The 6 size numbers count from 0.5 to 3 times a bar's own length or
	// thickness, see HudParts::Readout::Size.

	// How far right of its own spot by the ammo counter the Weapon CND bar
	// sits, below 0 to the left. Off its spot, the divider shows again.
	// HudCondition.h says how it stays on screen and what the MCM page shows.
	inline Live<float> fHudBarX{ "HUD", "fHudBarX", 0.0F };

	// How far up from its own spot the Weapon CND bar sits, below 0 down.
	inline Live<float> fHudBarY{ "HUD", "fHudBarY", 0.0F };

	// How long and how thick the Weapon CND bar is drawn.
	inline Live<float> fHudBarLength{ "HUD", "fHudBarLength", 1.0F };
	inline Live<float> fHudBarThickness{ "HUD", "fHudBarThickness", 1.0F };

	// Whether the Weapon CND bar hides with the ammo counter while the counter
	// has rounds to count, see HudCondition.h.
	inline Live<bool> bHudBarHidesWithCounter{ "HUD", "bHudBarHidesWithCounter", false };

	// Whether the Weapon CND bar stands alone at the place the MCM page shows
	// while a HUD mod hides the ammo counter, see HudCondition.h. When off, it
	// hides with the counter.
	inline Live<bool> bHudBarStandsAlone{ "HUD", "bHudBarStandsAlone", false };

	// Whether the CND word shows beside the Weapon CND bar. When off, only the
	// bar shows, at the same place.
	inline Live<bool> bHudBarShowsWord{ "HUD", "bHudBarShowsWord", true };

	// How far right of the dash's ammo digits the Power armor CND bar sits,
	// below 0 to the left. It still sways with the dash, see
	// PowerArmorCondition.h.
	inline Live<float> fPowerArmorBarX{ "HUD", "fPowerArmorBarX", 0.0F };

	// How far up from the dash's ammo digits it sits, below 0 down.
	inline Live<float> fPowerArmorBarY{ "HUD", "fPowerArmorBarY", 0.0F };

	// How long and how thick the Power armor CND bar is drawn.
	inline Live<float> fPowerArmorBarLength{ "HUD", "fPowerArmorBarLength", 1.0F };
	inline Live<float> fPowerArmorBarThickness{ "HUD", "fPowerArmorBarThickness", 1.0F };

	// Whether the CND word shows beside the Power armor CND bar. When off,
	// only the bar shows, at the same place.
	inline Live<bool> bPowerArmorBarShowsWord{ "HUD", "bPowerArmorBarShowsWord", true };

	// Whether the Pickup CND bar shows under the name in the prompt of a
	// weapon, armor or food lying around, see PickupBar.h.
	inline Live<bool> bPickupBar{ "HUD", "bPickupBar", true };

	// How far right of its own place under the name the Pickup CND bar sits,
	// below 0 to the left.
	inline Live<float> fPickupBarX{ "HUD", "fPickupBarX", 0.0F };

	// How far up from its own place it sits, below 0 down.
	inline Live<float> fPickupBarY{ "HUD", "fPickupBarY", 0.0F };

	// How long and how thick the Pickup CND bar is drawn.
	inline Live<float> fPickupBarLength{ "HUD", "fPickupBarLength", 1.0F };
	inline Live<float> fPickupBarThickness{ "HUD", "fPickupBarThickness", 1.0F };

	// Whether the CND word shows beside the Pickup CND bar. When off, only
	// the bar shows, at the same place.
	inline Live<bool> bPickupBarShowsWord{ "HUD", "bPickupBarShowsWord", true };
}
