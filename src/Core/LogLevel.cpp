#include "Core/Settings.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <string_view>

// NEC.log's level, the log level part of Settings.h.
namespace Settings
{
	namespace
	{
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

		std::string_view WordFor(REX::ELogLevel a_level)
		{
			const auto it = std::ranges::find(LEVELS, a_level, &Level::level);
			return it != std::end(LEVELS) ? it->word : "info"sv;
		}
	}

	REX::ELogLevel LogLevel()
	{
		return ParseLevel(sLogLevel.GetValue()).value_or(REX::ELogLevel::Info);
	}

	bool IsLogLevel(std::string_view a_word)
	{
		return ParseLevel(a_word).has_value();
	}

	REX::ELogLevel LogLevelNow()
	{
		const auto logger = spdlog::default_logger();
		if (!logger) {
			return LogLevel();
		}
		return static_cast<REX::ELogLevel>(std::min(static_cast<int>(logger->level()), static_cast<int>(REX::ELogLevel::Critical)));
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
		const auto  now = static_cast<int>(LogLevelNow());
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
		return WordFor(LogLevelNow());
	}

	std::string_view LogLevelDefaultWord()
	{
		return WordFor(ParseLevel(sLogLevel.GetValueDefault()).value_or(REX::ELogLevel::Info));
	}
}
