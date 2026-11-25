#pragma once

#include <cstddef>
#include <filesystem>
#include <format>
#include <string>

// Removing the numbered files past the count each log keeps, as NEC.log
// opens. Private to this folder.
namespace LogFiles
{
	// NEC.log, NEC.1.log and so on, by the number of starts back.
	[[nodiscard]] inline std::filesystem::path Numbered(const std::filesystem::path& a_path, std::size_t a_back)
	{
		if (a_back == 0) {
			return a_path;
		}
		auto numbered = a_path;
		numbered.replace_extension(std::format(L".{:d}", a_back) + a_path.extension().native());
		return numbered;
	}

	// Removes the numbered files past the a_kept earlier starts each log
	// keeps, such as NEC.3.log to NEC.10.log and NEC.trace.3.log to
	// NEC.trace.10.log at 1 kept. Only NEC's own log names in a_folder,
	// nothing else there. How many it removed, with the first file it could
	// not in a_failed.
	std::size_t Tidy(const std::filesystem::path& a_folder, std::size_t a_kept, std::string& a_failed);
}
