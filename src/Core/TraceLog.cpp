#include "Core/TraceLog.h"

#include "Core/LogFiles.h"
#include "Core/Settings.h"

#include <spdlog/details/os.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace TraceLog
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		// A block stays open while its lines come less than this far apart.
		// Long enough for a round to land and the Pip-Boy to rebuild its cards,
		// short enough that the next thing the player does starts its own.
		constexpr auto BLOCK_IDLE = std::chrono::milliseconds{ 1500 };

		// The column the thread is printed in, when a line leaves room for it.
		constexpr std::size_t THREAD_COLUMN = 100;

		// How many lines each file remembers for Once.
		constexpr std::size_t RECENT_LINES = 16;

		// A line Once wrote, and when it was last asked for.
		struct Recent
		{
			std::string       tag;
			std::string       text;
			Clock::time_point last;
		};

		// The block a file's lines go into. One per file, since a file is one
		// timeline written by several threads. The lock covers deciding where a
		// line belongs and writing it. Nothing but the file is touched under
		// it, so it cannot deadlock with the game's locks.
		struct Block
		{
			std::string       tag;
			std::size_t       thread{ 0 };
			Clock::time_point start;
			Clock::time_point last;
		};

		// One file and everything that belongs to it. Holding one file's lock
		// never keeps another file waiting.
		struct File
		{
			std::shared_ptr<spdlog::logger> logger;
			std::mutex                      lock;
			Block                           block;
			bool                            anyBlock{ false };

			// The lines Once wrote lately, the oldest written over first.
			std::array<Recent, RECENT_LINES> recent;
			std::size_t                      nextRecent{ 0 };

			// Every line First wrote since the last Mark, by its hash.
			std::unordered_set<std::size_t> firsts;

			// What Count has counted in the open block, in the order each
			// first came in, and the tag, time and thread of the last.
			std::vector<std::pair<std::string, std::uint32_t>> counts;
			std::string                                         countTag;
			Clock::time_point                                   counted;
			std::size_t                                         countThread{ 0 };

			// Counted under the lock, read without it, hence the atomic.
			std::atomic<std::uint32_t> blocks{ 0 };
		};

		File g_game;
		File g_ui;
		File g_npc;

		// Whether lines go through, set by Switch under the lock.
		std::mutex        g_switchLock;
		std::atomic<bool> g_open{ false };

		File& FileFor(detail::Target a_target)
		{
			switch (a_target) {
			case detail::Target::kUI:
				return g_ui;
			case detail::Target::kNpc:
				return g_npc;
			default:
				return g_game;
			}
		}

		// Where NEC.log is being written, asked of the logger LogFiles made
		// rather than worked out again. The file sink is rotating, or basic
		// when the earlier starts could not move along, so both are tried.
		std::filesystem::path MainLogPath()
		{
			const auto logger = spdlog::default_logger();
			if (!logger) {
				return {};
			}

			// Neither sink offers filename on a const object. Nothing writes
			// through the casts.
			for (const auto& sink : logger->sinks()) {
				if (auto* basic = dynamic_cast<spdlog::sinks::basic_file_sink_mt*>(sink.get())) {
					return basic->filename();
				}
				if (auto* rotating = dynamic_cast<spdlog::sinks::rotating_file_sink_mt*>(sink.get())) {
					return rotating->filename();
				}
			}

			return {};
		}

		// The time of day to the millisecond, in the local time zone.
		std::string ClockTime()
		{
			const auto now = std::chrono::system_clock::now();
			const auto tm = spdlog::details::os::localtime(std::chrono::system_clock::to_time_t(now));
			const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
			return std::format("{:02d}:{:02d}:{:02d}.{:03d}", tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
		}

		void Write(File& a_file, std::string a_line, std::size_t a_thread, bool a_showThread)
		{
			if (a_showThread) {
				if (a_line.size() + 2 <= THREAD_COLUMN) {
					a_line.resize(THREAD_COLUMN, ' ');
				} else {
					a_line += "  ";
				}
				std::format_to(std::back_inserter(a_line), "thread {:d}", a_thread);
			}
			a_file.logger->log(spdlog::level::trace, std::string_view{ a_line });
		}

		// From here to Locked, each expects the file's lock to be held by the
		// caller.

		bool BlockIsOpen(const File& a_file, Clock::time_point a_now)
		{
			return a_file.anyBlock && a_now - a_file.block.last <= BLOCK_IDLE;
		}

		// A line in the open block, stamped with how long after the header it
		// came, and with its thread where that is not the header's.
		void WriteInBlock(File& a_file, std::string_view a_tag, std::string_view a_text, std::size_t a_thread, Clock::time_point a_at)
		{
			const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(a_at - a_file.block.start).count();
			Write(a_file, std::format("{:>12}  {:<7} {:s}", std::format("+{:d}ms", ms), a_tag, a_text),
				a_thread, a_thread != a_file.block.thread);
		}

		// Ends the block with what Count counted in it, timed at the last
		// count: "counted 7818 rolled, 1031 already set".
		void WriteCounts(File& a_file)
		{
			if (a_file.counts.empty()) {
				return;
			}

			std::string text;
			for (const auto& [what, count] : a_file.counts) {
				std::format_to(std::back_inserter(text), "{:s}{:d} {:s}", text.empty() ? "" : ", ", count, what);
			}
			WriteInBlock(a_file, a_file.countTag, std::format("counted {:s}", text), a_file.countThread, a_file.counted);
			a_file.counts.clear();
			a_file.countTag.clear();
		}

		// The first column is 12 wide, the clock time on a header, and the
		// offset below is right aligned to it, so tags line up down the file.
		void OpenBlock(File& a_file, std::string_view a_tag, std::string_view a_text, std::size_t a_thread, Clock::time_point a_now)
		{
			WriteCounts(a_file);
			if (a_file.anyBlock) {
				a_file.logger->log(spdlog::level::trace, std::string_view{});
			}
			a_file.anyBlock = true;
			a_file.block = { std::string(a_tag), a_thread, a_now, a_now };
			a_file.blocks.fetch_add(1, std::memory_order_relaxed);
			Write(a_file, std::format("{:<12}  {:<7} {:s}", ClockTime(), a_tag, a_text), a_thread, true);
		}

		// A line into the open block, or a new block where none is open.
		void Put(File& a_file, std::string_view a_tag, std::string_view a_text, std::size_t a_thread, Clock::time_point a_now)
		{
			if (!BlockIsOpen(a_file, a_now)) {
				OpenBlock(a_file, a_tag, a_text, a_thread, a_now);
				return;
			}

			a_file.block.last = a_now;
			WriteInBlock(a_file, a_tag, a_text, a_thread, a_now);
		}

		// Hands a_write the file a line for a_target goes to, under its lock,
		// with the thread asking and the time, where the file is open.
		template <class F>
		void Locked(detail::Target a_target, F&& a_write)
		{
			auto& file = FileFor(a_target);
			if (!file.logger) {
				return;
			}

			const auto             thread = spdlog::details::os::thread_id();
			const std::scoped_lock l(file.lock);
			a_write(file, thread, Clock::now());
		}

		// Opens one file beside NEC.log by replacing its extension:
		// NEC.trace.log, NEC.ui.trace.log, NEC.npc.trace.log.
		std::shared_ptr<spdlog::logger> OpenFile(std::filesystem::path a_path, const char* a_extension, const char* a_name, std::size_t a_maxSize)
		{
			a_path.replace_extension(a_extension);

			std::shared_ptr<spdlog::logger> logger;
			try {
				// Keeps earlier game starts the way NEC.log does, see LogFiles.h.
				logger = std::make_shared<spdlog::logger>(a_name, LogFiles::OpenFile(a_path, a_maxSize));
			} catch (const std::exception& e) {
				REX::WARN("Could not open the bug report log {:s}: {:s}", a_path.string(), e.what());
				return nullptr;
			}

			logger->set_level(spdlog::level::trace);

			// Flushed a line at a time, so a crash keeps its last seconds.
			logger->flush_on(spdlog::level::trace);

			// Lines arrive already formatted, times, tags and threads included.
			logger->set_pattern("%v");

			REX::INFO("Bug report log: {:s}", a_path.string());
			return logger;
		}
	}

	void Open()
	{
		if (!Settings::bTraceLogs.GetValue()) {
			REX::INFO("The bug report logs are switched off.");
			return;
		}
		Switch(true);
	}

	void Switch(bool a_on)
	{
		const std::scoped_lock l{ g_switchLock };
		if (a_on && !g_game.logger && !g_ui.logger && !g_npc.logger) {
			const auto path = MainLogPath();
			if (path.empty()) {
				REX::WARN("No main log file to sit beside, so there are no bug report logs this session.");
				return;
			}
			g_game.logger = OpenFile(path, "trace.log", "trace", LogFiles::GAME_TRACE_SIZE);
			g_ui.logger = OpenFile(path, "ui.trace.log", "ui trace", LogFiles::UI_TRACE_SIZE);
			g_npc.logger = OpenFile(path, "npc.trace.log", "npc trace", LogFiles::NPC_TRACE_SIZE);
		}
		// Released after the loggers are in place, so a thread that reads the
		// gate open finds them.
		g_open.store(a_on && (g_game.logger || g_ui.logger || g_npc.logger), std::memory_order_release);
	}

	bool IsOpen()
	{
		return g_open.load(std::memory_order_acquire);
	}

	namespace detail
	{
		std::string Name(const RE::TESForm* a_form)
		{
			if (!a_form) {
				return "nobody";
			}

			// A reference is named as shown, which a renamed or leveled actor
			// need not share with its base record.
			const char* shown = nullptr;
			if (a_form->IsActor()) {
				shown = const_cast<RE::Actor*>(static_cast<const RE::Actor*>(a_form))->GetDisplayFullName();
			}
			const auto name = shown ? std::string_view{ shown } : RE::TESFullName::GetFullName(*a_form);
			return std::format("{:s} [{:08X}]", name.empty() ? "?"sv : name, a_form->formID);
		}

		void Begin(Target a_target, std::string_view a_tag, std::string_view a_text)
		{
			Locked(a_target, [&](File& a_file, std::size_t a_thread, Clock::time_point a_now) {
				OpenBlock(a_file, a_tag, a_text, a_thread, a_now);
			});
		}

		void Group(Target a_target, std::string_view a_tag, std::string_view a_text)
		{
			Locked(a_target, [&](File& a_file, std::size_t a_thread, Clock::time_point a_now) {
				if (!BlockIsOpen(a_file, a_now) || a_file.block.tag != a_tag) {
					OpenBlock(a_file, a_tag, a_text, a_thread, a_now);
				}
			});
		}

		void Line(Target a_target, std::string_view a_tag, std::string_view a_text)
		{
			Locked(a_target, [&](File& a_file, std::size_t a_thread, Clock::time_point a_now) {
				Put(a_file, a_tag, a_text, a_thread, a_now);
			});
		}

		void Once(Target a_target, std::string_view a_tag, std::string_view a_text)
		{
			Locked(a_target, [&](File& a_file, std::size_t a_thread, Clock::time_point a_now) {
				// Asking for it again before it expires resets the timer. It
				// expires once a block would close.
				for (auto& seen : a_file.recent) {
					if (a_now - seen.last <= BLOCK_IDLE && seen.tag == a_tag && seen.text == a_text) {
						seen.last = a_now;
						return;
					}
				}

				a_file.recent[a_file.nextRecent] = { std::string(a_tag), std::string(a_text), a_now };
				a_file.nextRecent = (a_file.nextRecent + 1) % RECENT_LINES;
				Put(a_file, a_tag, a_text, a_thread, a_now);
			});
		}

		void Mark(Target a_target, std::string_view a_tag, std::string_view a_text)
		{
			Locked(a_target, [&](File& a_file, std::size_t a_thread, Clock::time_point a_now) {
				a_file.firsts.clear();
				OpenBlock(a_file, a_tag, a_text, a_thread, a_now);
			});
		}

		void First(Target a_target, std::string_view a_tag, std::string_view a_text)
		{
			// The tag and the words together, separated by a character neither
			// contains.
			std::string key{ a_tag };
			key += '\n';
			key += a_text;
			const auto hash = std::hash<std::string>{}(key);

			Locked(a_target, [&](File& a_file, std::size_t a_thread, Clock::time_point a_now) {
				if (a_file.firsts.insert(hash).second) {
					Put(a_file, a_tag, a_text, a_thread, a_now);
				}
			});
		}

		void Count(Target a_target, std::string_view a_tag, std::string_view a_what)
		{
			Locked(a_target, [&](File& a_file, std::size_t a_thread, Clock::time_point a_now) {
				// A count keeps its block open, the way a line does.
				a_file.block.last = a_now;
				const auto known = std::ranges::find(a_file.counts, a_what, &std::pair<std::string, std::uint32_t>::first);
				if (known != a_file.counts.end()) {
					known->second++;
				} else {
					a_file.counts.emplace_back(std::string(a_what), 1U);
				}
				if (a_file.countTag.empty()) {
					a_file.countTag = a_tag;
				}
				a_file.counted = a_now;
				a_file.countThread = a_thread;
			});
		}

		std::uint32_t Blocks(Target a_target)
		{
			return FileFor(a_target).blocks.load(std::memory_order_relaxed);
		}
	}
}
