#include "Core/LogFiles.h"

#include "Core/Settings.h"

#include <REX/CONVERT.h>
#include <REX/W32/OLE32.h>
#include <REX/W32/SHELL32.h>
#include <share.h>
#include <spdlog/details/os.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/spdlog.h>

#include <cerrno>
#include <cstdio>
#include <exception>
#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
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

		// Tries to open a file and the wait between them, as spdlog makes, for
		// a file another program holds for a moment.
		constexpr int          OPEN_TRIES = 5;
		constexpr unsigned int OPEN_WAIT_MS = 10;

		// The wait before a refused rename is tried again. Windows can refuse
		// one for a moment, for example while an antivirus reads the file.
		constexpr unsigned int RENAME_WAIT_MS = 100;

		// -------------------------------------------------------------------
		// The file sink
		// -------------------------------------------------------------------

		// 1 log file, see the top of LogFiles.h for why NEC has its own. It
		// throws where spdlog's rotating file sink throws. The base class
		// locks around every line, since several game threads write through
		// it.
		class FileSink final : public spdlog::sinks::base_sink<std::mutex>
		{
		public:
			// Moves the start before along as it opens, unless the file is
			// empty. A file that grows past a_maxSize moves along again.
			FileSink(std::filesystem::path a_path, std::size_t a_maxSize) :
				path(std::move(a_path)),
				maxSize(a_maxSize)
			{
				Reopen(false);
				size = spdlog::details::os::filesize(file.get());
				if (size > 0) {
					MoveAlong();
				}
			}

			// Opens the file empty and never moves it along, as spdlog's
			// basic file sink does.
			explicit FileSink(std::filesystem::path a_path) :
				path(std::move(a_path))
			{
				Reopen(true);
			}

			// Set once as it opens, so reading it needs no lock.
			[[nodiscard]] const std::filesystem::path& Path() const noexcept { return path; }

		protected:
			void sink_it_(const spdlog::details::log_msg& a_msg) override
			{
				// No file after a move that failed to reopen it.
				if (!file) {
					return;
				}

				spdlog::memory_buf_t line;
				formatter_->format(a_msg, line);
				auto newSize = size + line.size();
				// The real size is asked only past the limit, since asking
				// costs. An empty file stays, so a full disk does not move
				// files along on every line.
				if (maxSize > 0 && newSize > maxSize) {
					flush_();
					if (spdlog::details::os::filesize(file.get()) > 0) {
						MoveAlong();
						newSize = line.size();
					}
				}
				if (std::fwrite(line.data(), 1, line.size(), file.get()) != line.size()) {
					spdlog::throw_spdlog_ex(std::format("Failed writing to file {:s}", Utf8(path)), errno);
				}
				size = newSize;
			}

			void flush_() override
			{
				if (file && std::fflush(file.get()) != 0) {
					spdlog::throw_spdlog_ex(std::format("Failed flush to file {:s}", Utf8(path)), errno);
				}
			}

		private:
			// Closes the file with the sink, also when a constructor throws.
			struct CloseFile
			{
				void operator()(std::FILE* a_file) const noexcept { std::fclose(a_file); }
			};

			// Opens the file to add to it, emptied first when a_empty, see
			// OPEN_TRIES.
			void Reopen(bool a_empty)
			{
				file.reset();
				for (int tries = 0; tries < OPEN_TRIES; ++tries) {
					if (tries > 0) {
						spdlog::details::os::sleep_for_millis(OPEN_WAIT_MS);
					}
					std::error_code error;
					std::filesystem::create_directories(path.parent_path(), error);
					if (a_empty) {
						auto* emptied = ::_wfsopen(path.c_str(), L"wb", _SH_DENYNO);
						if (!emptied) {
							continue;
						}
						std::fclose(emptied);
					}
					file.reset(::_wfsopen(path.c_str(), L"ab", _SH_DENYNO));
					if (file) {
						return;
					}
				}
				spdlog::throw_spdlog_ex(std::format("Failed opening file {:s} for writing", Utf8(path)), errno);
			}

			// NEC.log, NEC.1.log and so on, by the number of starts back.
			[[nodiscard]] std::filesystem::path Numbered(std::size_t a_back) const
			{
				if (a_back == 0) {
					return path;
				}
				auto numbered = path;
				numbered.replace_extension(std::format(L".{:d}", a_back) + path.extension().native());
				return numbered;
			}

			// Replaces a_to with a_from.
			[[nodiscard]] static bool Move(const std::filesystem::path& a_from, const std::filesystem::path& a_to) noexcept
			{
				::_wremove(a_to.c_str());
				return ::_wrename(a_from.c_str(), a_to.c_str()) == 0;
			}

			// Each file moves 1 number up and the oldest is dropped, then an
			// empty file starts. When a move fails the file is emptied anyway,
			// so it cannot grow past its limit, and this throws.
			void MoveAlong()
			{
				file.reset();
				std::string failed;
				int         why{ 0 };
				for (auto back = KEPT_STARTS; back > 0; --back) {
					const auto from = Numbered(back - 1);
					std::error_code error;
					if (!std::filesystem::exists(from, error)) {
						continue;
					}
					const auto to = Numbered(back);
					if (Move(from, to)) {
						continue;
					}
					spdlog::details::os::sleep_for_millis(RENAME_WAIT_MS);
					if (!Move(from, to)) {
						// Read before the reopen below can change it.
						why = errno;
						failed = std::format("Failed renaming {:s} to {:s}", Utf8(from), Utf8(to));
						break;
					}
				}
				Reopen(true);
				size = 0;
				if (!failed.empty()) {
					spdlog::throw_spdlog_ex(failed, why);
				}
			}

			const std::filesystem::path           path;
			const std::size_t                     maxSize{ 0 };
			std::unique_ptr<std::FILE, CloseFile> file;
			std::size_t                           size{ 0 };
		};

		// -------------------------------------------------------------------
		// Opening a log
		// -------------------------------------------------------------------

		// A file sink, and the WARN to write once the log is open, empty when
		// the start before moved along.
		struct Opened
		{
			spdlog::sink_ptr sink;
			std::string      warning;
		};

		// The file sink for a_path. The start before moves along as it opens,
		// and an empty file stays where it is. When a move fails, the sink has
		// already emptied the file, and a plain sink opens it empty.
		Opened Open(const std::filesystem::path& a_path, std::size_t a_maxSize)
		{
			const auto name = Utf8(a_path);
			try {
				return { std::make_shared<FileSink>(a_path, a_maxSize), {} };
			} catch (const std::exception& e) {
				try {
					return { std::make_shared<FileSink>(a_path),
						std::format("Could not move {:s} along to keep it, so the last start's lines are lost and it starts empty: {:s}", name, e.what()) };
				} catch (const std::exception&) {
					// A read only file cannot be opened but can be renamed,
					// which keeps it and frees the name. A file another
					// program holds open cannot.
					auto aside = a_path;
					aside.replace_extension("old.log");
					std::error_code error;
					std::filesystem::rename(a_path, aside, error);
					if (error) {
						throw;
					}
					return { std::make_shared<FileSink>(a_path, a_maxSize),
						std::format("Could not write to {:s}, so it moved to {:s} and an empty one starts: {:s}", name, Utf8(aside), e.what()) };
				}
			}
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
			// Windows gives Documents with backslashes and the folders after it
			// join with forward slashes. make_preferred turns all of them into
			// backslashes, so every path NEC.log prints reads the same,
			// including the ones in bug report logs and the sink's reasons.
			path.make_preferred();
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
				const auto where = path.empty() ? std::string{ MAIN_FILE } : Utf8(path);
				REX::WARN("Could not open {:s}, so this game start writes no log file: {:s}", where, noFile);
			} else if (!file.warning.empty()) {
				REX::WARN("{:s}", file.warning);
			}
		} catch (...) {
			// Nothing is left to say it to, and a log is not worth ending the
			// game over.
		}
	}

	spdlog::sink_ptr OpenFile(const std::filesystem::path& a_path, std::size_t a_maxSize)
	{
		auto file = Open(a_path, a_maxSize);
		if (!file.warning.empty()) {
			REX::WARN("{:s}", file.warning);
		}
		return std::move(file.sink);
	}

	std::filesystem::path FileOf(const spdlog::sink_ptr& a_sink)
	{
		const auto* file = dynamic_cast<const FileSink*>(a_sink.get());
		return file ? file->Path() : std::filesystem::path{};
	}

	std::string Utf8(const std::filesystem::path& a_path)
	{
		// Stays empty for an empty path, the only case where the conversion
		// fails.
		std::string text;
		REX::UTF16_TO_UTF8(a_path.native(), text);
		return text;
	}
}
