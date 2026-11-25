#include "Core/LogFiles/Tidy.h"

#include "Core/LogFiles/LogFiles.h"

#include <format>
#include <string_view>
#include <system_error>

namespace LogFiles
{
	namespace
	{
		// The highest number a kept file has ever had, NEC.10.log, since NEC
		// once kept 10 earlier starts. Tidy looks up to it.
		constexpr std::size_t HIGHEST_NUMBER = 10;

		// Every log that keeps earlier starts, all beside NEC.log. A new log
		// that opens through OpenFile goes here too, so Tidy finds its files.
		constexpr std::wstring_view KEEPING[]{
			L"NEC.log",
			L"NEC.papyrus.log",
			L"NEC.trace.log",
			L"NEC.ui.trace.log",
			L"NEC.npc.trace.log",
			L"NEC.food.trace.log",
			L"NEC.papyrus.trace.log",
		};
	}

	std::size_t Tidy(const std::filesystem::path& a_folder, std::size_t a_kept, std::string& a_failed)
	{
		std::size_t removed = 0;
		for (const auto name : KEEPING) {
			const auto file = a_folder / name;
			for (auto back = a_kept + 1; back <= HIGHEST_NUMBER; ++back) {
				const auto numbered = Numbered(file, back);
				std::error_code error;
				if (std::filesystem::remove(numbered, error)) {
					++removed;
				} else if (error && a_failed.empty()) {
					a_failed = std::format("{:s}: {:s}", Shown(numbered), error.message());
				}
			}
		}
		return removed;
	}
}
