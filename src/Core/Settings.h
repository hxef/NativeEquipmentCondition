#pragma once

#include "Core/Plugin.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

// The plugin's settings, read from NEC.ini beside NEC.dll in Data\F4SE\Plugins.
// NEC_custom.ini in the same folder holds the player's changes, so an update
// writes over nothing of theirs. A key in neither file keeps the default
// written here.
//
// CommonLibF4's setting store reads both files once, as the game starts, and
// nothing reads them again while the game runs. Read a setting where it is
// used, never copy it out. Every bool and float is a Live one, so the MCM page
// can change it while game threads read it. The page also keeps the change in
// NEC_custom.ini, see UI/Mcm/Mcm.h.
//
// A setting added here is added to publish/NEC.ini too, with a line saying
// what it does.
namespace Settings
{
	// Where a setting sits in NEC.ini, which is also its id on the MCM page.
	struct Named
	{
		std::string_view section;
		std::string_view key;
	};

	// A setting the MCM page can change while game threads read it. Every
	// read and write goes through std::atomic_ref, so a hook reads the old
	// value or the new one and never half of each.
	template <class T>
	class Live : public REX::TIniSetting<T>, public Named
	{
	public:
		Live(std::string_view a_section, std::string_view a_key, T a_default) :
			REX::TIniSetting<T>(a_section, a_key, a_default), Named{ a_section, a_key }
		{}

		[[nodiscard]] T GetValue() const
		{
			return std::atomic_ref{ const_cast<T&>(this->m_value) }.load(std::memory_order_relaxed);
		}

		void SetValue(T a_value) { std::atomic_ref{ this->m_value }.store(a_value, std::memory_order_relaxed); }

		// NEC.ini's pass keeps its value as the default only. The pass after
		// it sees both files, so the value is set once, see Load in
		// Settings.cpp.
		void Load(void* a_data, bool a_isBase) override
		{
			if (a_isBase) {
				REX::Impl::IniSettingLoad<T>(a_data, section, key, this->m_valueDefault, this->m_valueDefault);
				return;
			}
			T value{};
			REX::Impl::IniSettingLoad<T>(a_data, section, key, value, this->m_valueDefault);
			SetValue(value);
		}
	};

	// Reads both files, once as the game starts. Runs before the log opens,
	// so Report says what was read afterwards.
	void Load();

	// Logs which files were read and a sLogLevel that is no level, then the
	// Settings line. Once, as the log opens.
	void Report();

	// Logs the Settings line, every setting as it stands on one line, then
	// the lines of what is left to other mods, what NEC shares with them and
	// what that leaves of each setting while there is any, see
	// CallPatch::Summary. With a_always false those lines follow only when
	// they read differently from the last ones written, as after a change
	// from the MCM page.
	void ReportLine(bool a_always = true);

	// Logs 1 change of a setting, whatever made it: "bJam: true to false, from
	// the MCM page." At info, or at the log's own level when that is higher,
	// so no level hides it.
	void LogChange(std::string_view a_key, std::string_view a_old, std::string_view a_new, std::string_view a_from);

	// The level NEC.log opens at, from sLogLevel. An unknown word reads as
	// info.
	[[nodiscard]] REX::ELogLevel LogLevel();

	// Sets NEC.log to the page's level a_index on the spot. The page's levels
	// are the entries of LEVELS in Settings.cpp marked for it, in
	// config.json's order. The MCM page keeps the word with its other changes.
	void SetLogLevel(std::size_t a_index);

	// The page's level for the level NEC.log is at now. A level the page does
	// not list gives its nearest, critical gives error.
	[[nodiscard]] std::size_t LogLevelIndex();

	// The word for the level NEC.log is at now, as the Settings line prints
	// it and NEC_custom.ini keeps it.
	[[nodiscard]] std::string_view LogLevelWord();

	// The word for the level NEC.ini gives, the same way.
	[[nodiscard]] std::string_view LogLevelDefaultWord();

	// Writes key=a_text under [a_section] in NEC_custom.ini, changing that
	// key's line alone, see IniText.h, and makes the file where there is
	// none. False when it cannot read the file it would change, the game
	// would not read the edit, or it cannot write.
	bool Keep(std::string_view a_section, std::string_view a_key, std::string_view a_text);

	// Takes a_key's line out of [a_section] in NEC_custom.ini, so NEC.ini's
	// value stands, and logs that. A file without the line, or no file, is
	// left alone. False as for Keep.
	bool Drop(std::string_view a_section, std::string_view a_key);

	// Features. Each is the switch on a row of Features.cpp.

	// Worn guns can jam.
	inline Live<bool> bJam{ "Features", "bJam", true };

	// Worn automatic weapons fire slower, in anyone's hands, see FireRate.h.
	inline Live<bool> bFireRate{ "Features", "bFireRate", true };

	// Worn weapons fill the VATS critical meter slower, and an NPC's worn
	// weapon lands fewer critical hits, see CritMeter.h.
	inline Live<bool> bCritMeter{ "Features", "bCritMeter", true };

	// Weapons and armor spawn already worn, see SpawnCondition.h. Provenance
	// measures the load order for it at every data load, whatever the switch
	// says, so it works as soon as it is switched on in play.
	inline Live<bool> bSpawnCondition{ "Features", "bSpawnCondition", true };

	// Traders who deal in weapons, armor or clothing repair them for caps.
	inline Live<bool> bVendorRepair{ "Features", "bVendorRepair", true };

	// Loading screen tips about condition.
	inline Live<bool> bLoadingTips{ "Features", "bLoadingTips", true };

	// The CND bar on the HUD, in the ammo counter and on the power armor dash.
	inline Live<bool> bHudCondition{ "Features", "bHudCondition", true };

	// Condition meters on the rows of the quick container.
	inline Live<bool> bQuickContainer{ "Features", "bQuickContainer", true };

	// A workbench's confirmation box shows more of a long list and scrolls it,
	// see ConfirmScroll.h.
	inline Live<bool> bConfirmScroll{ "Features", "bConfirmScroll", true };

	// The inspect screen at a trader shows the price for the side the item
	// is on, see InspectPrice.h.
	inline Live<bool> bInspectPrice{ "Features", "bInspectPrice", true };

	// Balance. Each tunes a feature and is read where it is used.

	// What every weapon's wear rate is multiplied by, see WeaponWear/Rate.cpp.
	inline Live<float> fWearRateMult{ "Balance", "fWearRateMult", 1.0F };

	// What every piece of armor's wear rate is multiplied by, see
	// ArmorWear/Rate.cpp.
	inline Live<float> fArmorWearRateMult{ "Balance", "fArmorWearRateMult", 1.0F };

	// What a weapon at nothing still hits for, as a share of its full damage,
	// see HealthDamage/Curve.h.
	inline Live<float> fDamageFloor{ "Balance", "fDamageFloor", 0.66F };

	// What a piece of armor at nothing still protects for, as a share of its
	// full resistances, see ArmorRating.h.
	inline Live<float> fArmorFloor{ "Balance", "fArmorFloor", 0.66F };

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

	// What a repair at a workbench costs, against the mod as tuned, see
	// Workbench/Cost.h.
	inline Live<float> fBenchCostMult{ "Balance", "fBenchCostMult", 1.0F };

	// What a trader asks for a repair, against the mod as tuned, see
	// VendorRepair/Quote.h.
	inline Live<float> fTraderPriceMult{ "Balance", "fTraderPriceMult", 1.0F };

	// HUD. Where the CND bars sit, in pixels of the HUD, which is 1280
	// across and 720 down on any screen. Read every frame, so a change shows
	// at once. 0 and 0 is the bar's own place.

	// How far right of the ammo divider the CND bar sits, below 0 to the
	// left. Off the divider, the divider shows again, see HudCondition.h.
	inline Live<float> fHudBarX{ "HUD", "fHudBarX", 0.0F };

	// How far up from the ammo divider the CND bar sits, below 0 down.
	inline Live<float> fHudBarY{ "HUD", "fHudBarY", 0.0F };

	// How far right of the dash's ammo digits the power armor CND bar sits,
	// below 0 to the left. It still sways with the dash, see
	// PowerArmorCondition.h.
	inline Live<float> fPowerArmorBarX{ "HUD", "fPowerArmorBarX", 0.0F };

	// How far up from the dash's ammo digits it sits, below 0 down.
	inline Live<float> fPowerArmorBarY{ "HUD", "fPowerArmorBarY", 0.0F };

	// Log. Read once, as the log is opened. The MCM page sets the level and
	// switches the trace logs on the spot, see SetLogLevel and
	// TraceLog::Switch.

	// What NEC.log writes: trace, debug, info, warning or error. Info is what
	// the plugin says about itself. Debug adds a line for each recheck that
	// finds nothing changed, and CommonLibF4's own lines.
	inline REX::TIniSetting<std::string> sLogLevel{ "Log", "sLogLevel", "info" };

	// Whether the 3 trace logs are written, see TraceLog.h. They grow fast, so
	// they are off unless a bug is being reported.
	inline Live<bool> bTraceLogs{ "Log", "bTraceLogs", false };

	// The 10 switches under [Features] and the 13 numbers under [Balance] and
	// [HUD], in NEC.ini's order, for Report and the MCM page.
	[[nodiscard]] std::span<Live<bool>* const>  Switches();
	[[nodiscard]] std::span<Live<float>* const> Numbers();
}
