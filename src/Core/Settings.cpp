#include "Core/Settings.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/IniText.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <mutex>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

namespace Settings
{
	namespace
	{
		// Both files sit beside the DLL, and the game runs from its own folder.
		// Literals, since a store keeps only a view of what it is handed.
		constexpr const char* BASE_FILE = "Data/F4SE/Plugins/NEC.ini";
		constexpr const char* USER_FILE = "Data/F4SE/Plugins/NEC_custom.ini";

		// Keep and Drop write this first and rename it over USER_FILE, so a
		// write cut short never leaves the player's file half written.
		constexpr const char* NEW_FILE = "Data/F4SE/Plugins/NEC_custom.ini.new";

		// Changes from the page arrive one after another, and each reads the
		// file the last one wrote. Keep and Drop share this one lock.
		std::mutex g_userFileLock;

		Live<bool>* const SWITCHES[]{
			&bJam,
			&bFireRate,
			&bCritMeter,
			&bSpawnCondition,
			&bVendorRepair,
			&bLoadingTips,
			&bHudCondition,
			&bQuickContainer,
			&bConfirmScroll,
			&bInspectPrice,
		};

		const Number NUMBERS[]{
			&fWearRateMult,
			&fArmorWearRateMult,
			&fDamageFloor,
			&fArmorFloor,
			&fValueExponent,
			&fFireRateFloor,
			&fCritMeterFloor,
			&fBenchCostMult,
			&iFreeMendAbove,
			&fTraderPriceMult,
			&fHudBarX,
			&fHudBarY,
			&fPowerArmorBarX,
			&fPowerArmorBarY,
		};

		bool Exists(const char* a_file)
		{
			std::error_code ec;
			return std::filesystem::exists(a_file, ec);
		}

		// The levels by the words spdlog uses for them, matched in any case.
		// page marks the 5 the MCM page steps through, in its order.
		struct Level
		{
			std::string_view word;
			REX::ELogLevel   level;
			bool             page;
		};

		constexpr Level LEVELS[]{
			{ "trace", REX::ELogLevel::Trace, true },
			{ "debug", REX::ELogLevel::Debug, true },
			{ "info", REX::ELogLevel::Info, true },
			{ "warning", REX::ELogLevel::Warning, true },
			{ "warn", REX::ELogLevel::Warning, false },
			{ "error", REX::ELogLevel::Error, true },
			{ "critical", REX::ELogLevel::Critical, false },
		};

		std::optional<REX::ELogLevel> ParseLevel(std::string_view a_word)
		{
			for (const auto& [word, level, page] : LEVELS) {
				if (std::ranges::equal(a_word, word, [](char a_lhs, char a_rhs) {
						return std::tolower(static_cast<unsigned char>(a_lhs)) == std::tolower(static_cast<unsigned char>(a_rhs));
					})) {
					return level;
				}
			}
			return std::nullopt;
		}

		// The level NEC.log is at now, which the MCM page can change, or the
		// one it opens at before it is open.
		REX::ELogLevel LevelNow()
		{
			const auto logger = spdlog::default_logger();
			if (!logger) {
				return LogLevel();
			}
			return static_cast<REX::ELogLevel>(std::min(static_cast<int>(logger->level()), static_cast<int>(REX::ELogLevel::Critical)));
		}

		std::string_view WordFor(REX::ELogLevel a_level)
		{
			const auto it = std::ranges::find(LEVELS, a_level, &Level::level);
			return it != std::end(LEVELS) ? it->word : "info"sv;
		}

		// The whole file, or false when any of it cannot be read or it
		// changes size while it is read.
		bool ReadWhole(const char* a_file, std::string& a_text)
		{
			std::error_code ec;
			const auto      size = std::filesystem::file_size(a_file, ec);
			if (ec) {
				return false;
			}
			std::ifstream file{ a_file, std::ios::binary };
			a_text.resize(size);
			return file && file.read(a_text.data(), static_cast<std::streamsize>(size)) &&
			       file.gcount() == static_cast<std::streamsize>(size) && file.peek() == std::ifstream::traits_type::eof();
		}

		bool WriteWhole(const char* a_file, std::string_view a_text)
		{
			std::ofstream file{ a_file, std::ios::binary | std::ios::trunc };
			file.write(a_text.data(), static_cast<std::streamsize>(a_text.size()));
			file.close();
			return !file.fail();
		}

		// Who has a switch's part, short for the Settings line: the DLLs,
		// another mod, or the game version.
		std::string Owned(std::span<const CallPatch::Owner> a_owners)
		{
			std::string text;
			for (const auto& name : CallPatch::OwnerNames(a_owners, "another mod")) {
				text += text.empty() ? name : ", " + name;
			}
			return text.empty() ? std::format("game version {:s} not supported", CallPatch::GameVersion()) : text;
		}

		// A setting's mark on the Settings line while pieces it works through
		// are off: (no effect), or (some) while it still does part of its job.
		// An idle setting gets none, see CallPatch::Effect. The summary, see
		// CallPatch::Summary, says which pieces.
		std::string_view Mark(const Named& a_setting)
		{
			const auto effect = CallPatch::EffectOf(a_setting);
			return effect.idle ? ""sv : effect.None() ? "(no effect)"sv : !effect.off.empty() ? "(some)"sv : ""sv;
		}

		// A switch as the Settings line prints it. A switch left to another
		// mod says to whom, and one that is on carries its mark.
		std::string Shown(const Live<bool>& a_switch)
		{
			if (const auto yield = CallPatch::YieldOf(a_switch)) {
				std::vector<CallPatch::Owner> owners;
				for (const auto& cause : yield->causes) {
					for (const auto& owner : cause.owners) {
						if (std::ranges::find(owners, owner) == owners.end()) {
							owners.push_back(owner);
						}
					}
				}
				std::ranges::sort(owners, CallPatch::Before);
				return std::format("false({:s})", Owned(owners));
			}
			return a_switch.GetValue() ? std::format("true{:s}", Mark(a_switch)) : "false";
		}

		// A number as the Settings line prints it.
		std::string Shown(float a_value)
		{
			return std::format("{:g}", a_value);
		}

		std::string Shown(std::int32_t a_value)
		{
			return std::format("{:d}", a_value);
		}

		// At info, or at the log's own level when that is higher, so no level
		// hides the line.
		void Say(const std::string& a_line, std::source_location a_where = std::source_location::current())
		{
			REX::Impl::Log(a_where, std::max(LevelNow(), REX::ELogLevel::Info), a_line);
		}

		// What an edit of NEC_custom.ini came to.
		enum class Edited
		{
			kFailed,
			kSame,  // the edit left the text as it was, so nothing is written
			kWritten,
		};

		// Edits NEC_custom.ini through a_edit, which is handed the whole text
		// and gives it back changed, or nothing when the game would not read
		// the edit.
		template <class Edit>
		Edited Rewrite(std::string_view a_key, Edit a_edit)
		{
			const std::scoped_lock l{ g_userFileLock };

			// A file that cannot be read whole is never written over, or the
			// player's other keys would be lost. With no file the text is
			// empty.
			std::error_code ec;
			const auto      exists = std::filesystem::exists(USER_FILE, ec);
			std::string     text;
			if (ec || (exists && !ReadWhole(USER_FILE, text))) {
				REX::WARN("NEC_custom.ini could not be read, so the change to {:s} is not saved in it.", a_key);
				return Edited::kFailed;
			}

			const std::optional<std::string> edited = a_edit(std::string_view{ text });
			if (!edited) {
				REX::WARN("NEC_custom.ini is not plain text or has a line that is only [, so the change to {:s} is not saved in it.", a_key);
				return Edited::kFailed;
			}
			if (*edited == text) {
				return Edited::kSame;
			}
			if (!WriteWhole(NEW_FILE, *edited)) {
				REX::WARN("NEC_custom.ini.new could not be written, so the change to {:s} is not saved in NEC_custom.ini.", a_key);
				std::filesystem::remove(NEW_FILE, ec);
				return Edited::kFailed;
			}
			std::filesystem::rename(NEW_FILE, USER_FILE, ec);
			if (ec) {
				REX::WARN("NEC_custom.ini could not be replaced, so the change to {:s} is not saved in it: {:s}", a_key, ec.message());
				std::filesystem::remove(NEW_FILE, ec);
				return Edited::kFailed;
			}
			return Edited::kWritten;
		}
	}

	std::span<Live<bool>* const> Switches()
	{
		return SWITCHES;
	}

	std::span<const Number> Numbers()
	{
		return NUMBERS;
	}

	void Load()
	{
		// The base file first and the custom file over it, read into one
		// table, so the second pass sees every key and sets each value once.
		// With no custom file the base file is read twice for that pass.
		auto* store = REX::FIniSettingStore::GetSingleton();
		store->Init(BASE_FILE, Exists(USER_FILE) ? USER_FILE : BASE_FILE);
		store->Load();
	}

	void LogChange(std::string_view a_key, std::string_view a_old, std::string_view a_new, std::string_view a_from)
	{
		Say(std::format("{:s}: {:s} to {:s}, {:s}.", a_key, a_old, a_new, a_from));
	}

	REX::ELogLevel LogLevel()
	{
		return ParseLevel(sLogLevel.GetValue()).value_or(REX::ELogLevel::Info);
	}

	void SetLogLevel(std::size_t a_index)
	{
		std::size_t index = 0;
		for (const auto& [word, level, page] : LEVELS) {
			if (!page || index++ != a_index) {
				continue;
			}
			// spdlog keeps both levels as atomics, so this is safe while other
			// threads log. The cast is the one CommonLibF4's F4SE::Init uses.
			if (const auto logger = spdlog::default_logger()) {
				logger->set_level(static_cast<spdlog::level::level_enum>(level));
				logger->flush_on(static_cast<spdlog::level::level_enum>(level));
			}
			return;
		}
	}

	std::size_t LogLevelIndex()
	{
		const auto  now = static_cast<int>(LevelNow());
		std::size_t index = 0;
		std::size_t nearest = 0;
		int         gap = INT_MAX;
		for (const auto& [word, level, page] : LEVELS) {
			if (!page) {
				continue;
			}
			if (std::abs(static_cast<int>(level) - now) < gap) {
				gap = std::abs(static_cast<int>(level) - now);
				nearest = index;
			}
			index++;
		}
		return nearest;
	}

	std::string_view LogLevelWord()
	{
		return WordFor(LevelNow());
	}

	std::string_view LogLevelDefaultWord()
	{
		return WordFor(ParseLevel(sLogLevel.GetValueDefault()).value_or(REX::ELogLevel::Info));
	}

	bool Keep(std::string_view a_section, std::string_view a_key, std::string_view a_text)
	{
		return Rewrite(a_key, [&](std::string_view a_file) {
			return IniText::WithKey(a_file, a_section, a_key, a_text);
		}) != Edited::kFailed;
	}

	bool Drop(std::string_view a_section, std::string_view a_key)
	{
		const auto edited = Rewrite(a_key, [&](std::string_view a_file) {
			return IniText::WithoutKey(a_file, a_section, a_key);
		});
		if (edited == Edited::kWritten) {
			Say(std::format("NEC_custom.ini no longer sets {:s}, so NEC.ini's value stands.", a_key));
		}
		return edited != Edited::kFailed;
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
		ReportLine();
	}

	void ReportLine(bool a_always)
	{
		// Every key as it stands now, before any limit, in NEC.ini's order and
		// spelling.
		std::string line = "Settings: [Features]";
		for (const auto* on : SWITCHES) {
			std::format_to(std::back_inserter(line), " {:s}={:s}", on->key, Shown(*on));
		}
		std::string_view section;
		for (const auto& entry : NUMBERS) {
			std::visit([&](const auto* a_number) {
				if (a_number->section != section) {
					section = a_number->section;
					std::format_to(std::back_inserter(line), " [{:s}]", section);
				}
				std::format_to(std::back_inserter(line), " {:s}={:s}{:s}", a_number->key, Shown(a_number->GetValue()), Mark(*a_number));
			}, entry);
		}
		std::format_to(std::back_inserter(line), " [Log] sLogLevel={:s} bTraceLogs={}", LogLevelWord(), bTraceLogs.GetValue());
		REX::INFO("{:s}", line);
		// Above info NEC.log drops these lines, so no summary is kept and the
		// next one at info is written in full.
		if (LevelNow() > REX::ELogLevel::Info) {
			CallPatch::KeepSummary({});
			return;
		}
		// Every summary written is kept, so the next one compares with it.
		const auto summary = CallPatch::Summary();
		if (CallPatch::KeepSummary(summary) || a_always) {
			for (const auto& text : summary) {
				REX::INFO("{:s}", text);
			}
		}
	}
}
