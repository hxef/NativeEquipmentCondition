#pragma once

#include <spdlog/common.h>

#include <cstddef>
#include <filesystem>

// NEC.log and the 3 bug report logs keep the logs of the 10 game starts
// before. As a log opens, the file of the start before moves to NEC.1.log,
// NEC.trace.1.log and so on, so a restart after a crash still has the lines
// that led to it. The moving is done by the rotating file sink of spdlog, the
// library CommonLibF4 logs through.
//
// NEC opens NEC.log here and not through F4SE::Init. Its rotation option
// builds the sink without catching, and the sink throws when an old file
// cannot move, for example while another program holds it open. Inside
// F4SE::Init, which is noexcept, that would end the game at startup. Here a
// file that cannot move loses only the start before: the log starts empty,
// with a WARN in NEC.log, and the older numbered files stay. A file that
// cannot be written, such as a read only one, moves to <name>.old.log
// (NEC.old.log, NEC.trace.old.log) and an empty one starts.
namespace LogFiles
{
	// 1 megabyte, for the sizes below.
	inline constexpr std::size_t MB = 1024 * 1024;

	// A log this big moves along within 1 game start, a guard against a
	// runaway log. Measured over a 32 minute start with the bug report logs
	// on: NEC.log 28 KB, NEC.npc.trace.log 2 KB, NEC.trace.log 167 KB and
	// NEC.ui.trace.log 304 KB.
	inline constexpr std::size_t MAIN_SIZE = 2 * MB;
	inline constexpr std::size_t NPC_TRACE_SIZE = 2 * MB;
	inline constexpr std::size_t GAME_TRACE_SIZE = 10 * MB;
	inline constexpr std::size_t UI_TRACE_SIZE = 10 * MB;

	// Opens NEC.log in Documents\My Games\<save folder>\F4SE the way F4SE::Init
	// would, and makes it the log every REX line goes to. Call it right after
	// F4SE::Init, which names the save folder.
	void OpenMain() noexcept;

	// The file sink for 1 log, keeping the 10 starts before. It falls back as
	// the top of this file says, with a WARN, and throws when the last
	// fallback fails too, for example while another program holds the file
	// open.
	[[nodiscard]] spdlog::sink_ptr OpenFile(const std::filesystem::path& a_path, std::size_t a_maxSize);
}
