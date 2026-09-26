#include "Core/Settings.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string_view>
#include <system_error>

namespace Settings
{
	namespace
	{
		// Both files sit beside the DLL, and the game runs from its own folder.
		// Literals, since a store keeps only a view of what it is handed.
		constexpr const char* BASE_FILE = "Data/F4SE/Plugins/NEC.ini";
		constexpr const char* USER_FILE = "Data/F4SE/Plugins/NEC_custom.ini";

		bool Exists(const char* a_file)
		{
			std::error_code ec;
			return std::filesystem::exists(a_file, ec);
		}

		// The levels by the words spdlog uses for them, matched in any case.
		struct Level
		{
			std::string_view word;
			REX::ELogLevel   level;
		};

		constexpr Level LEVELS[]{
			{ "trace", REX::ELogLevel::Trace },
			{ "debug", REX::ELogLevel::Debug },
			{ "info", REX::ELogLevel::Info },
			{ "warning", REX::ELogLevel::Warning },
			{ "warn", REX::ELogLevel::Warning },
			{ "error", REX::ELogLevel::Error },
			{ "critical", REX::ELogLevel::Critical },
		};

		std::optional<REX::ELogLevel> ParseLevel(std::string_view a_word)
		{
			for (const auto& [word, level] : LEVELS) {
				if (std::ranges::equal(a_word, word, [](char a_lhs, char a_rhs) {
						return std::tolower(static_cast<unsigned char>(a_lhs)) == std::tolower(static_cast<unsigned char>(a_rhs));
					})) {
					return level;
				}
			}
			return std::nullopt;
		}

		// The base file first and the custom file over it. A missing one is
		// passed over.
		void Read(REX::FIniSettingStore& a_store)
		{
			a_store.Init(BASE_FILE, USER_FILE);
			a_store.Load();
		}
	}

	EveryLoad* EveryLoad::GetSingleton()
	{
		static EveryLoad singleton;
		return &singleton;
	}

	void Load()
	{
		Read(*REX::FIniSettingStore::GetSingleton());
		Reload();
	}

	void Reload()
	{
		Read(*EveryLoad::GetSingleton());
	}

	REX::ELogLevel LogLevel()
	{
		return ParseLevel(sLogLevel.GetValue()).value_or(REX::ELogLevel::Info);
	}

	void Report()
	{
		const auto base = Exists(BASE_FILE);
		const auto user = Exists(USER_FILE);
		REX::INFO("{:s}{:s}",
			base ? "NEC.ini read" : "No NEC.ini beside the DLL, so the defaults stand",
			user ? ", and NEC_custom.ini on top of it." : ", and there is no NEC_custom.ini.");

		const auto level = sLogLevel.GetValue();
		if (!ParseLevel(level)) {
			REX::WARN("sLogLevel \"{:s}\" is not a level, so the log is at info.", level);
		}

		// Every key as read, before any limit, in NEC.ini's order and spelling.
		// Features and Log stand as the game started, Balance as this load.
		REX::INFO("Settings: [Features] bJam={} bFireRate={} bCritMeter={} bSpawnCondition={} bVendorRepair={} "
				  "bLoadingTips={} bHudCondition={} bQuickContainer={} bConfirmScroll={} "
				  "[Balance] fWearRateMult={:g} fArmorWearRateMult={:g} fDamageFloor={:g} fArmorFloor={:g} "
				  "fValueExponent={:g} fFireRateFloor={:g} fCritMeterFloor={:g} fBenchCostMult={:g} fTraderPriceMult={:g} "
				  "[Log] sLogLevel={:s} bTraceLogs={}",
			bJam.GetValue(), bFireRate.GetValue(), bCritMeter.GetValue(), bSpawnCondition.GetValue(), bVendorRepair.GetValue(),
			bLoadingTips.GetValue(), bHudCondition.GetValue(), bQuickContainer.GetValue(), bConfirmScroll.GetValue(),
			fWearRateMult.GetValue(), fArmorWearRateMult.GetValue(), fDamageFloor.GetValue(), fArmorFloor.GetValue(),
			fValueExponent.GetValue(), fFireRateFloor.GetValue(), fCritMeterFloor.GetValue(), fBenchCostMult.GetValue(), fTraderPriceMult.GetValue(),
			level, bTraceLogs.GetValue());
	}
}
