#pragma once

#include <optional>
#include <string>
#include <string_view>

// Changes or takes out 1 key in the text of an ini file and leaves every other
// line as it is. NEC_custom.ini is the player's file, so their comments, blank
// lines and order stay theirs.
//
// The text is read the way CommonLibF4 reads the file. Section and key names
// match in any case, and spaces around a name or around = do not count. A
// line that starts with ; or # is a comment. A section given twice is 1
// section, and of a key given twice the last one counts. A [ with no ] on its
// line ends the section before it.
//
// It uses the standard library alone, so it builds and can be tested outside
// the game.
namespace IniText
{
	// a_text with a_key=a_value under [a_section]. Every line of that key in
	// the section is rewritten. With none, the line goes after the section's
	// last key, and with no such section, the section goes at the end.
	// Nothing when the reader would miss the edit: the text holds a NUL byte,
	// as a file saved as UTF-16 does, or a line that is only [.
	[[nodiscard]] std::optional<std::string> WithKey(std::string_view a_text, std::string_view a_section, std::string_view a_key, std::string_view a_value);

	// a_text without the lines of a_key under [a_section], so the reader
	// falls back to NEC.ini. Every other line stays as it was, and a text
	// without the key comes back as it was. Nothing in the same cases as
	// WithKey.
	[[nodiscard]] std::optional<std::string> WithoutKey(std::string_view a_text, std::string_view a_section, std::string_view a_key);
}
