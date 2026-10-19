#pragma once

#include "Core/Text/Lines.h"

#include <optional>
#include <string>
#include <string_view>

// What a language file must be for NEC to use it: its encoding, and the {n}
// slots of each line. xmake/texts.lua checks the shipped files by the same
// rules. Private to this folder.
namespace Text
{
	// a_bytes as UTF-8 text, or nothing and why NEC reads none of it: UTF-8
	// with or without its mark, or UTF-16 LE with its mark, as Notepad saves
	// "Unicode". An ANSI file is refused, not guessed at, since the same bytes
	// mean other letters on a Polish and a Russian PC.
	[[nodiscard]] std::optional<std::string> Decoded(std::string_view a_bytes, std::string& a_why);

	// Why a file's text cannot stand in for a_key's English, or nothing when
	// it can: its slots are a bare {0}, {1} or {2}, the same set as English's
	// in any order, so std::vformat can always fill them.
	[[nodiscard]] std::optional<std::string> SlotFault(std::string_view a_text, Key a_key);
}
