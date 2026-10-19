#include "UI/Mcm/Bridge.h"

#include "Core/Text/Text.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The cap on a grey row. MCM's list is 450 px tall, and with NEC's page in
// None mode, see Layout.cpp, a row taller than that never shows: at most 18
// lines of 24.8 px, or 21 lines of Japanese or Chinese at 21 px. The cap
// keeps 1 line under each, so 17 and 20. A line is counted as so many
// characters a line, by language. Checked on about 10000 of NEC's rows
// against the widths of the game's fonts, that count was never below the
// lines a row wraps to. A language file's [Layout] sets both for its
// language, and NEC holds them to 10 to 100 characters and 5 to 20 lines.
namespace Mcm
{
	namespace
	{
		struct Fit
		{
			std::string_view code;
			std::size_t      perLine;
			std::size_t      budget;
		};

		constexpr Fit FITS[]{
			{ "en", 62, 17 },
			{ "fr", 64, 17 },
			{ "de", 58, 17 },
			{ "it", 66, 17 },
			{ "es", 62, 17 },
			{ "esmx", 62, 17 },
			{ "ptbr", 62, 17 },
			{ "pl", 60, 17 },
			{ "ru", 54, 17 },
			{ "ja", 27, 20 },
			{ "cn", 26, 20 },
		};

		// The player's language file's [Layout], else its row, else English's.
		Fit FitNow()
		{
			const auto language = Text::LayoutNow();
			const auto it = std::ranges::find(FITS, language.code, &Fit::code);
			const auto& row = it != std::end(FITS) ? *it : FITS[0];
			return { language.code, language.lineWidth ? language.lineWidth : row.perLine, language.rowLines ? language.rowLines : row.budget };
		}

		// The lines a_line takes, from its characters before html escaping.
		// A character of 2 to 4 bytes counts once, so Cyrillic and Chinese
		// count right.
		std::size_t LinesOf(std::string_view a_line, const Fit& a_fit)
		{
			const auto characters = static_cast<std::size_t>(
				std::ranges::count_if(a_line, [](char a_byte) { return (static_cast<unsigned char>(a_byte) & 0xC0) != 0x80; }));
			return std::max<std::size_t>(1, (characters + a_fit.perLine - 1) / a_fit.perLine);
		}
	}

	std::size_t LinesOf(std::string_view a_line)
	{
		return LinesOf(a_line, FitNow());
	}

	// a_lines whole while they fit. Else the lines from the first while they
	// fit in all but 1 line, the first that does not fit in its a_shorter
	// form where it has one that fits, always the first line, then the line
	// saying NEC.log names the rest.
	Kept Capped(std::vector<std::string> a_lines, const Shorter& a_shorter)
	{
		const auto fit = FitNow();
		std::size_t used = 0;
		for (const auto& line : a_lines) {
			used += LinesOf(line, fit);
		}
		if (used <= fit.budget) {
			const auto whole = a_lines.size();
			return { std::move(a_lines), whole };
		}
		Kept kept;
		used = 0;
		for (std::size_t i = 0; i < a_lines.size(); i++) {
			const auto lines = LinesOf(a_lines[i], fit);
			if (used + lines < fit.budget) {
				used += lines;
				kept.lines.push_back(std::move(a_lines[i]));
				kept.whole++;
				continue;
			}
			if (auto shorter = a_shorter ? a_shorter(i, fit.budget - 1 - used) : std::nullopt) {
				kept.lines.push_back(std::move(*shorter));
			} else if (kept.lines.empty()) {
				kept.lines.push_back(std::move(a_lines[i]));
				kept.whole++;
			}
			break;
		}
		kept.lines.push_back(Text::MenuNoteRest());
		return kept;
	}
}
