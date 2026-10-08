#include "Core/LogFiles.h"

#include "Core/Settings.h"

#include <REX/W32/OLE32.h>
#include <REX/W32/SHELL32.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

#include <exception>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace LogFiles
{
	namespace
	{
		// How many earlier starts each log keeps, NEC.1.log to NEC.10.log.
		// Enough to reach back over an evening of crashes and restarts.
		constexpr std::size_t KEPT_STARTS = 10;

		// NEC.log's file name.
		constexpr std::string_view MAIN_FILE = "NEC.log";

		// The logger name and line layout F4SE::Init gives its log.
		constexpr const char* MAIN_LOGGER = "global";
		constexpr const char* MAIN_PATTERN = "[%T.%e] [%=5t] [%L] %v";

		// A file sink, and why the start before was lost, empty when it moved
		// along.
		struct Opened
		{
			spdlog::sink_ptr sink;
			std::string lost;
		};

		// The file sink for a_path. The start before moves along as it opens,
		// and an empty file stays where it is. When a move fails, spdlog has
		// already emptied the file, and a plain sink opens it empty.
		Opened Open(const std::filesystem::path& a_path, std::size_t a_maxSize)
		{
			// mt: the sink locks, since several game threads write through it.
			const auto name = a_path.string();
			try {
				return { std::make_shared<spdlog::sinks::rotating_file_sink_mt>(name, a_maxSize, KEPT_STARTS, true), {} };
			} catch (const std::exception& e) {
				return { std::make_shared<spdlog::sinks::basic_file_sink_mt>(name, true), e.what() };
			}
		}

		// The 1 line that says a log lost the start before.
		void SayLost(const std::filesystem::path& a_path, std::string_view a_why)
		{
			REX::WARN("Could not move {:s} along to keep it, so the start before is lost and it starts empty: {:s}", a_path.string(), a_why);
		}

		// Documents\My Games\<save folder>\F4SE\NEC.log, worked out as
		// F4SE::Init does. Empty, with a_why filled in, when Windows or F4SE
		// names no folder.
		std::filesystem::path MainPath(std::string& a_why)
		{
			const auto saveFolder = F4SE::GetSaveFolderName();
			if (saveFolder.empty()) {
				a_why = "F4SE named no save folder";
				return {};
			}

			wchar_t* buffer{ nullptr };
			const auto result = REX::W32::SHGetKnownFolderPath(REX::W32::FOLDERID_Documents, REX::W32::KF_FLAG_DEFAULT, nullptr, &buffer);
			const std::unique_ptr<wchar_t[], decltype(&REX::W32::CoTaskMemFree)> documents(buffer, REX::W32::CoTaskMemFree);
			if (!documents || result != 0) {
				a_why = "Windows named no Documents folder";
				return {};
			}

			std::filesystem::path path = documents.get();
			path /= std::format("My Games/{}/F4SE", saveFolder);
			path /= MAIN_FILE;
			return path;
		}
	}

	void OpenMain() noexcept
	{
		try {
			// The debugger's output window, as in every CommonLibF4 log.
			std::vector<spdlog::sink_ptr> sinks{ std::make_shared<spdlog::sinks::msvc_sink_mt>() };

			std::string noFile;
			Opened file;
			const auto path = MainPath(noFile);
			if (!path.empty()) {
				try {
					file = Open(path, MAIN_SIZE);
					sinks.push_back(file.sink);
				} catch (const std::exception& e) {
					noFile = e.what();
				}
			}

			const auto level = static_cast<spdlog::level::level_enum>(Settings::LogLevel());
			auto logger = std::make_shared<spdlog::logger>(MAIN_LOGGER, sinks.begin(), sinks.end());
			logger->set_level(level);
			logger->flush_on(level);

			spdlog::set_default_logger(std::move(logger));
			spdlog::set_pattern(MAIN_PATTERN);

			REX::INFO("{} v{}", F4SE::GetPluginName(), F4SE::GetPluginVersion());
			if (!noFile.empty()) {
				const auto where = path.empty() ? std::string{ MAIN_FILE } : path.string();
				REX::WARN("Could not open {:s}, so this game start writes no log file: {:s}", where, noFile);
			} else if (!file.lost.empty()) {
				SayLost(path, file.lost);
			}
		} catch (...) {
			// Nothing is left to say it to, and a log is not worth ending the
			// game over.
		}
	}

	spdlog::sink_ptr OpenFile(const std::filesystem::path& a_path, std::size_t a_maxSize)
	{
		auto file = Open(a_path, a_maxSize);
		if (!file.lost.empty()) {
			SayLost(a_path, file.lost);
		}
		return std::move(file.sink);
	}
}
