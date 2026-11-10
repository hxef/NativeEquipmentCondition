#include "Papyrus/Log.h"

#include "Core/LogFiles.h"
#include "Core/ScriptEvents.h"
#include "Core/TraceLog.h"

#include <spdlog/details/os.h>
#include <spdlog/spdlog.h>

#include <array>
#include <atomic>
#include <chrono>
#include <ctime>
#include <exception>
#include <format>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Papyrus::Log
{
	namespace
	{
		constexpr std::string_view FILE_NAME = "NEC.papyrus.log";

		constexpr std::array<std::string_view, 3> HEAD{
			"; NEC's Papyrus API: each time the game builds its script VM, which scripts each event reached first, and the",
			"; errors in scripts' calls to NEC. NEC.papyrus.trace.log adds, with Bug report logs on in MCM, the calls and the",
			"; events.",
		};

		// A warning of 1 function gets a line at most this often.
		constexpr std::chrono::seconds EVERY{ 10 };

		using Clock = std::chrono::steady_clock;

		// 1 warning of 1 function, since its last line.
		struct Counted
		{
			std::uint32_t     waiting{ 0 };
			Clock::time_point lastLine;
			std::string       lastAt;  // the clock time of the last line
			std::string       detail;  // the last one's, for the line as the game ends
		};

		thread_local std::string_view t_calling;

		// A warning Hold kept back, as WarnFor was given it.
		struct Held
		{
			std::string   event;
			Warning       warning;
			std::string   detail;
			std::uint32_t count;
		};

		thread_local bool              t_holding{ false };
		thread_local std::vector<Held> t_held;

		// Guards the file and the tables. Taken by script calls and the
		// event task, never under an inventory lock.
		std::mutex                                           g_lock;
		std::shared_ptr<spdlog::logger>                      g_file;
		bool                                                 g_opened{ false };
		std::map<std::pair<Warning, std::string>, Counted>   g_counts;
		std::map<std::string, std::uint32_t, std::less<>>    g_reached;  // each event's game it was said in
		std::atomic<std::uint32_t>                           g_functions{ 0 };
		std::atomic<std::uint32_t>                           g_tasklets{ 0 };

		// -------------------------------------------------------------------
		// The file
		// -------------------------------------------------------------------

		// The time of day to the second, in the local time zone.
		std::string ClockTime()
		{
			const auto tm = spdlog::details::os::localtime(std::time(nullptr));
			return std::format("{:02d}:{:02d}:{:02d}", tm.tm_hour, tm.tm_min, tm.tm_sec);
		}

		// The file, opened beside NEC.log as its first line comes. Keeps the
		// 10 starts before as NEC.log does, see LogFiles.h. Under g_lock.
		spdlog::logger* File()
		{
			if (g_opened) {
				return g_file.get();
			}
			g_opened = true;
			auto path = LogFiles::MainFile();
			if (path.empty()) {
				REX::WARN("No main log file to sit beside, so there is no {:s} this session.", FILE_NAME);
				return nullptr;
			}
			path.replace_filename(FILE_NAME);
			try {
				g_file = std::make_shared<spdlog::logger>("papyrus", LogFiles::OpenFile(path, LogFiles::PAPYRUS_LOG_SIZE));
			} catch (const std::exception& e) {
				REX::WARN("Could not open {:s}: {:s}", LogFiles::Shown(path), e.what());
				return nullptr;
			}
			g_file->set_level(spdlog::level::trace);
			// Flushed a line at a time, so a crash keeps its last lines.
			g_file->flush_on(spdlog::level::trace);
			g_file->set_pattern("%v");
			for (const auto line : HEAD) {
				g_file->info("{:s}", line);
			}
			REX::INFO("Papyrus log: {:s}", LogFiles::Shown(path));
			return g_file.get();
		}

		// 1 line at the clock time a_at. Under g_lock.
		void Say(std::string_view a_at, std::string_view a_text)
		{
			if (auto* file = File()) {
				file->info("[{:s}] {:s}", a_at, a_text);
			}
		}

		// -------------------------------------------------------------------
		// The sentences
		// -------------------------------------------------------------------

		std::string Sentence(Warning a_warning, std::string_view a_function, std::string_view a_detail)
		{
			switch (a_warning) {
			case Warning::kNone:
				return std::format("NEC.{:s} was given None where it needs an item.", a_function);
			case Warning::kGone:
				return std::format("NEC.{:s} was given a handle to a stack that no longer exists ({:s}).", a_function, a_detail);
			case Warning::kUntracked:
				return std::format("NEC.{:s} was asked to write {:s}.", a_function, a_detail);
			case Warning::kRefused:
				return std::format("NEC.{:s} was given {:s}, which it does not take, so it did nothing.", a_function, a_detail);
			case Warning::kClamped:
				return std::format("NEC.{:s} was given {:s}.", a_function, a_detail);
			case Warning::kLoading:
				return std::format("NEC.{:s} was called while a save loaded, so it did nothing.", a_function);
			case Warning::kFrozen:
				return std::format("NEC.{:s} was called while the game froze its scripts for a save, a load or a quit, so it did nothing.",
					a_function);
			case Warning::kEventBusy:
				return std::format("{:s} waited 2 s for the inventory of {:s}, so its item is None.", a_function, a_detail);
			case Warning::kDropped:
				return "The events waiting for scripts were too many, so the newest was dropped.";
			}
			return {};
		}

		std::string Times(std::uint32_t a_count)
		{
			return a_count == 1 ? "1 time" : std::format("{:d} times", a_count);
		}
	}

	// -------------------------------------------------------------------
	// The lines
	// -------------------------------------------------------------------

	void Calling(std::string_view a_function)
	{
		t_calling = a_function;
		ScriptEvents::KickIfWaiting();
	}

	void Warn(Warning a_warning, std::string_view a_detail, std::uint32_t a_count)
	{
		WarnFor(t_calling, a_warning, a_detail, a_count);
	}

	void WarnFor(std::string_view a_event, Warning a_warning, std::string_view a_detail, std::uint32_t a_count)
	{
		if (a_count == 0) {
			return;
		}
		if (t_holding) {
			t_held.push_back({ std::string{ a_event }, a_warning, std::string{ a_detail }, a_count });
			return;
		}
		const auto             now = Clock::now();
		const std::scoped_lock l(g_lock);
		auto [at, first] = g_counts.try_emplace({ a_warning, std::string{ a_event } });
		auto& counted = at->second;
		counted.waiting += a_count;
		counted.detail = a_detail;
		if (!first && now - counted.lastLine < EVERY) {
			return;
		}
		const auto clock = ClockTime();
		const auto since = first ? std::string{} : std::format(" since {:s}", counted.lastAt);
		Say(clock, std::format("Warning: {:s} {:s}{:s}.", Sentence(a_warning, a_event, a_detail), Times(counted.waiting), since));
		counted.waiting = 0;
		counted.lastLine = now;
		counted.lastAt = clock;
	}

	void Hold()
	{
		t_held.clear();
		t_holding = true;
	}

	void Release(bool a_keep)
	{
		t_holding = false;
		auto held = std::move(t_held);
		t_held.clear();
		if (a_keep) {
			for (const auto& one : held) {
				WarnFor(one.event, one.warning, one.detail, one.count);
			}
		}
	}

	void Bound(std::uint32_t a_functions, std::uint32_t a_tasklets)
	{
		g_functions = a_functions;
		g_tasklets = a_tasklets;
		REX::INFO("Papyrus: the script NEC has {:d} functions bound.", a_functions);
		TraceLog::Papyrus::Begin("BIND", "functions={:d} tasklet={:d}", a_functions, a_tasklets);
	}

	void Checked(std::optional<std::int32_t> a_pex, std::int32_t a_dll)
	{
		TraceLog::Papyrus::Begin("SCRIPT", "NEC.pex={:s} dll={:d}", a_pex ? std::to_string(*a_pex) : "none", a_dll);
		std::string pex;
		if (!a_pex) {
			pex = "NEC.pex did not answer, so it is missing or broken. Every call of another mod into NEC fails.";
		} else if (*a_pex != a_dll) {
			pex = std::format("NEC.pex is {:d} and NEC.dll is {:d}, so some functions may not bind. Install NEC.pex from the same release.",
				*a_pex, a_dll);
		} else {
			pex = std::format("NEC.pex is {:d}, the same as NEC.dll.", a_dll);
		}
		const bool same = a_pex && *a_pex == a_dll;
		if (same) {
			REX::INFO("Papyrus: {:s}", pex);
		} else {
			REX::WARN("Papyrus: {:s}", pex);
		}

		const std::scoped_lock l(g_lock);
		const auto             clock = ClockTime();
		Say(clock, std::format("The game built its script VM. {:d} functions of the script NEC are bound, {:d} of them answer at once.",
			g_functions.load(), g_tasklets.load()));
		Say(clock, same ? pex : "Warning: " + pex);
	}

	void Reached(std::string_view a_event, std::string_view a_scripts)
	{
		const auto             game = ScriptEvents::Game();
		const std::scoped_lock l(g_lock);
		auto [at, first] = g_reached.try_emplace(std::string{ a_event }, game);
		if (!first && at->second == game) {
			return;
		}
		at->second = game;
		Say(ClockTime(), std::format("{:s} reached its scripts for the first time this game: {:s}.", a_event, a_scripts));
	}

	void EndGame()
	{
		const std::scoped_lock l(g_lock);
		const auto             clock = ClockTime();
		for (const auto& [key, counted] : g_counts) {
			if (counted.waiting > 0) {
				Say(clock, std::format("Warning: {:s} {:s} since {:s}, as the game ended.", Sentence(key.first, key.second, counted.detail),
					Times(counted.waiting), counted.lastAt));
			}
		}
		g_counts.clear();
	}
}
