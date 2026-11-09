#pragma once

#include "Core/Plugin.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <variant>

// The plugin's settings, read from NEC.ini beside NEC.dll in Data\F4SE\Plugins.
// NEC_custom.ini in the same folder holds the player's changes, so an update
// writes over nothing of theirs. A key in neither file keeps the default
// written here. A key not written here is never read, so a key an older NEC
// had, left in either file, does nothing and NEC.log says nothing of it.
//
// CommonLibF4's setting store reads both files once, as the game starts, and
// nothing reads them again while the game runs. Read a setting where it is
// used, never copy it out. Every bool, float and whole number is a Live one,
// so the MCM page can change it while game threads read it. The page also keeps
// the change in NEC_custom.ini, see UI/Mcm/Mcm.h.
//
// A setting added here or in SettingsTuning.h is added to publish/NEC.ini too,
// with a line saying what it does, and to EVERY_SWITCH or NUMBERS in
// SettingsLine.cpp in NEC.ini's order. A setting left out of those is missing
// from the Settings line, and neither the MCM page nor a script can read or
// change it. A switch on a row of Features.cpp goes in SWITCHES too, and any
// other switch gets its place on the Settings line in ReportLine.
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

	// Logs 1 change of a setting, whatever made it: "bSpawnCondition: true to
	// false, from the MCM page." At info, or at the log's own level when that
	// is higher, so no level hides it.
	void LogChange(std::string_view a_key, std::string_view a_old, std::string_view a_new, std::string_view a_from);

	// The level NEC.log opens at, from sLogLevel. An unknown word reads as
	// info.
	[[nodiscard]] REX::ELogLevel LogLevel();

	// Whether a_word names a level, in any case.
	[[nodiscard]] bool IsLogLevel(std::string_view a_word);

	// The level NEC.log is at now, which the MCM page can change, or the one
	// it opens at before it is open.
	[[nodiscard]] REX::ELogLevel LogLevelNow();

	// Sets NEC.log to the page's level a_index on the spot. The page's levels
	// are the entries of LEVELS in LogLevel.cpp marked for it, in
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

	// The rows of the repair boxes at a workbench and at a trader in columns,
	// see Repair/Columns/Columns.h. When off, each row is 1 centred line, as
	// vanilla draws its boxes. No feature's row: it rides on Workbench repairs
	// and Trader repairs. On by default.
	inline Live<bool> bRepairBoxColumns{ "Features", "bRepairBoxColumns", true };

	// Loading screen tips about condition.
	inline Live<bool> bLoadingTips{ "Features", "bLoadingTips", true };

	// The Weapon CND bar on the HUD, by the ammo counter.
	inline Live<bool> bHudCondition{ "Features", "bHudCondition", true };

	// The Power armor CND bar on the HUD, on the power armor dash. Its row
	// goes with bHudCondition's part, see Features.cpp.
	inline Live<bool> bPowerArmorBar{ "Features", "bPowerArmorBar", true };

	// Condition meters on the rows of the quick container.
	inline Live<bool> bQuickContainer{ "Features", "bQuickContainer", true };

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

	// Loot. Tunes Worn loot and is read at each roll.

	// How far every ordinary loot roll moves toward worn, below 0, or toward
	// new, above 0, as a share of the way from -100 to 100, see
	// SpawnCondition/Band.cpp. 0 is the plain roll.
	inline Live<std::int32_t> iLootShift{ "Loot", "iLootShift", 0 };
}

// [Balance] and [HUD]. Each setting joins the setting store as it is made, so
// they are included here, between [Loot] and [Log], in NEC.ini's order.
#include "Core/SettingsTuning.h"

namespace Settings
{
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

	// A number under [Features], [Loot], [Balance] or [HUD]: a float, or a
	// whole number where the setting only takes whole steps.
	using Number = std::variant<Live<float>*, Live<std::int32_t>*>;

	// The 8 switches under [Features] and the 44 numbers under [Features],
	// [Loot], [Balance] and [HUD], in NEC.ini's order, for Report and the MCM
	// page.
	[[nodiscard]] std::span<Live<bool>* const> Switches();
	[[nodiscard]] std::span<const Number>      Numbers();

	// Whether a_id names a_setting as the MCM page and the Papyrus API write
	// it, key:section, such as bSpawnCondition:Features. In any case, since
	// Papyrus keeps 1 spelling of each text for the whole game, so a script's
	// id can arrive as bspawncondition:features.
	[[nodiscard]] bool IsIdOf(const Named& a_setting, std::string_view a_id);

	// The switch a_id names, any of the 20 in NEC.ini, or nullptr for an id
	// NEC does not know.
	[[nodiscard]] Live<bool>* SwitchById(std::string_view a_id);

	// The number a_id names, a float or a whole number as T asks, or nullptr
	// for an id NEC does not know or a number of the other kind.
	template <class T>
	[[nodiscard]] Live<T>* NumberById(std::string_view a_id)
	{
		for (const auto& number : Numbers()) {
			auto* const* live = std::get_if<Live<T>*>(&number);
			if (live && IsIdOf(**live, a_id)) {
				return *live;
			}
		}
		return nullptr;
	}
}
