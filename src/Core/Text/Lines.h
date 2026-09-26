#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <string_view>

// One sentence in every language, and picking the player's. Private to this
// folder.
namespace Text
{
	// One sentence in one language. code is what sLanguage says. The first
	// row, English, is the fallback. A table holds the languages the game is
	// sold in, plus Simplified Chinese, which the executable names.
	struct Line
	{
		std::string_view code;
		const char*      text;
	};

	// Full condition, as a whole percent.
	inline constexpr std::uint32_t FULL = 100;

	// The row in the player's language, or the English one.
	[[nodiscard]] const char* Pick(std::span<const Line> a_lines);

	// That row with a_args filled into its {0} {1} slots. The arguments are
	// taken by reference because std::make_format_args wants lvalues.
	template <class... T>
	[[nodiscard]] std::string Say(std::span<const Line> a_lines, const T&... a_args)
	{
		return std::vformat(Pick(a_lines), std::make_format_args(a_args...));
	}
}
