#pragma once

#include <spdlog/common.h>

#include <cstddef>
#include <filesystem>
#include <string>

// NEC.log and the 3 bug report logs keep the logs of the 10 game starts
// before. As a log opens, the file of the start before moves to NEC.1.log,
// NEC.trace.1.log and so on, so a restart after a crash still has the lines
// that led to it.
//
// The files are opened here by a file sink of NEC's own, which moves them
// along the way the rotating file sink of spdlog, the library CommonLibF4
// logs through, would. spdlog's file sinks take the path in the Windows code
// page, so a Documents folder with letters outside it, such as Cyrillic on a
// Western Windows, would get no log. NEC's sink opens the file by the path
// as Windows gives it, which holds any letter, and NEC.log prints every path
// as Shown below says.
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
	// on: NEC.log 28 KB, NEC.trace.log 167 KB and NEC.ui.trace.log 304 KB.
	// NEC.npc.trace.log takes about 0.8 KB for each NPC shot or blow that
	// wears, so a big fight writes a few MB. A move runs while the line is
	// written, sometimes inside an NPC's inventory write, so it should be rare.
	inline constexpr std::size_t MAIN_SIZE = 2 * MB;
	inline constexpr std::size_t NPC_TRACE_SIZE = 10 * MB;
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

	// The file a_sink writes to, empty for a sink this file did not open.
	[[nodiscard]] std::filesystem::path FileOf(const spdlog::sink_ptr& a_sink);

	// a_path as NEC's logs print it, from My Games on, so a log a player posts
	// never names their Windows user. In UTF-8, as every log line is written.
	// Never throws, unlike path::string, which throws on a letter outside the
	// Windows code page.
	[[nodiscard]] std::string Shown(const std::filesystem::path& a_path);
}
