#pragma once

#include "Core/Plugin.h"

#include <string>

// The plugin's settings, read from NEC.ini beside NEC.dll in Data\F4SE\Plugins.
// NEC_custom.ini in the same folder holds the player's changes, so an update
// writes over nothing of theirs. A key in neither file keeps the default
// written here.
//
// CommonLibF4's setting store does the reading. There are 2 stores because the
// settings are read at 2 times. Features and Log are read once, as the game
// starts. Balance is read again on every save load, so tuning a number takes a
// load and no restart. Read a setting where it is used, never copy it out.
//
// A setting added here is added to NEC.ini too, with a line saying what it
// does.
namespace Settings
{
	// The store the Balance settings sit in, read again on every save load.
	class EveryLoad : public REX::FIniSettingStore
	{
	public:
		static EveryLoad* GetSingleton();
	};

	// Reads both files into both stores. Runs before the log opens, so Report
	// says what was read afterwards.
	void Load();

	// Reads both files into the Balance store alone.
	void Reload();

	// Logs which files were read and every setting on one line.
	void Report();

	// The level NEC.log opens at, from sLogLevel. An unknown word reads as
	// info.
	[[nodiscard]] REX::ELogLevel LogLevel();

	// Features. Each is the switch on a row of Features.cpp.

	// Worn guns can jam.
	inline REX::TIniSetting<bool> bJam{ "Features", "bJam", true };

	// Worn automatic weapons fire slower, in anyone's hands, see FireRate.h.
	inline REX::TIniSetting<bool> bFireRate{ "Features", "bFireRate", true };

	// Worn weapons fill the VATS critical meter slower, and an NPC's worn
	// weapon lands fewer critical hits, see CritMeter.h.
	inline REX::TIniSetting<bool> bCritMeter{ "Features", "bCritMeter", true };

	// Weapons and armor spawn already worn, see SpawnCondition.h. Provenance
	// measures the load order for it and for nothing else, so the switch
	// covers both.
	inline REX::TIniSetting<bool> bSpawnCondition{ "Features", "bSpawnCondition", true };

	// Traders who deal in weapons, armor or clothing repair them for caps.
	inline REX::TIniSetting<bool> bVendorRepair{ "Features", "bVendorRepair", true };

	// Loading screen tips about condition.
	inline REX::TIniSetting<bool> bLoadingTips{ "Features", "bLoadingTips", true };

	// The CND bar on the HUD, in the ammo counter and on the power armor dash.
	inline REX::TIniSetting<bool> bHudCondition{ "Features", "bHudCondition", true };

	// Condition meters on the rows of the quick container.
	inline REX::TIniSetting<bool> bQuickContainer{ "Features", "bQuickContainer", true };

	// A workbench's confirmation box shows more of a long list and scrolls it,
	// see ConfirmScroll.h.
	inline REX::TIniSetting<bool> bConfirmScroll{ "Features", "bConfirmScroll", true };

	// The inspect screen at a trader shows the price for the side the item
	// is on, see InspectPrice.h.
	inline REX::TIniSetting<bool> bInspectPrice{ "Features", "bInspectPrice", true };

	// Balance. Read again on every save load.

	// What every weapon's wear rate is multiplied by, see WeaponWear/Rate.cpp.
	inline REX::TIniSetting<float, EveryLoad> fWearRateMult{ "Balance", "fWearRateMult", 1.0F };

	// What every piece of armor's wear rate is multiplied by, see
	// ArmorWear/Rate.cpp.
	inline REX::TIniSetting<float, EveryLoad> fArmorWearRateMult{ "Balance", "fArmorWearRateMult", 1.0F };

	// What a weapon at nothing still hits for, as a share of its full damage,
	// see HealthDamage/Curve.h.
	inline REX::TIniSetting<float, EveryLoad> fDamageFloor{ "Balance", "fDamageFloor", 0.66F };

	// What a piece of armor at nothing still protects for, as a share of its
	// full resistances, see ArmorRating.h.
	inline REX::TIniSetting<float, EveryLoad> fArmorFloor{ "Balance", "fArmorFloor", 0.66F };

	// What a worn item is worth, as its condition raised to this power, see
	// ItemValue.cpp.
	inline REX::TIniSetting<float, EveryLoad> fValueExponent{ "Balance", "fValueExponent", 1.5F };

	// How fast an automatic weapon at nothing fires, as a share of its own
	// rate, see FireRate.cpp.
	inline REX::TIniSetting<float, EveryLoad> fFireRateFloor{ "Balance", "fFireRateFloor", 0.75F };

	// How much of the VATS critical meter a shot from a weapon at nothing
	// fills, and how much of its critical chance an NPC's weapon at nothing
	// keeps, see CritMeter.cpp.
	inline REX::TIniSetting<float, EveryLoad> fCritMeterFloor{ "Balance", "fCritMeterFloor", 0.5F };

	// What a repair at a workbench costs, against the mod as tuned, see
	// Workbench/Cost.h.
	inline REX::TIniSetting<float, EveryLoad> fBenchCostMult{ "Balance", "fBenchCostMult", 1.0F };

	// What a trader asks for a repair, against the mod as tuned, see
	// VendorRepair/Quote.h.
	inline REX::TIniSetting<float, EveryLoad> fTraderPriceMult{ "Balance", "fTraderPriceMult", 1.0F };

	// Log. Read once, as the log is opened.

	// What NEC.log writes: trace, debug, info, warning or error. Info is what
	// the plugin says about itself, debug adds CommonLibF4.
	inline REX::TIniSetting<std::string> sLogLevel{ "Log", "sLogLevel", "info" };

	// Whether the 3 trace logs are written, see TraceLog.h. They grow fast, so
	// they are off unless somebody is working on the mod.
	inline REX::TIniSetting<bool> bTraceLogs{ "Log", "bTraceLogs", false };
}
