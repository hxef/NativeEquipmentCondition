#pragma once

#include <cstddef>
#include <string_view>

// Reading UTF-8 text 1 letter at a time, strictly: no overlong form, no half
// of a UTF-16 pair and nothing past U+10FFFF. The language files and a typed
// preset name are both checked with it.
namespace Utf8
{
	// The bytes of the whole letter a_text starts with, or 0 when it starts
	// with none.
	[[nodiscard]] std::size_t LetterSize(std::string_view a_text);

	// Where a_text stops being UTF-8, or npos when all of it is.
	[[nodiscard]] std::size_t BadAt(std::string_view a_text);

	// The code point of a_letter, 1 whole letter as LetterSize measured it.
	[[nodiscard]] char32_t CodePoint(std::string_view a_letter);

	// Whether a_letter, 1 whole letter, is a control character of ASCII or
	// of Latin-1: below U+0020, or U+007F to U+009F.
	[[nodiscard]] bool IsControl(std::string_view a_letter);
}
