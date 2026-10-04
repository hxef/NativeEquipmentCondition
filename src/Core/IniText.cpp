#include "Core/IniText.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace IniText
{
	namespace
	{
		// A UTF-8 byte order mark, which the reader skips.
		constexpr std::string_view BOM = "\xEF\xBB\xBF";

		struct Line
		{
			std::string_view text;    // without its ending
			std::string_view ending;  // \r\n, \n, \r, or nothing on the last line
		};

		// Ends a line at \r\n, \n or a lone \r, as the reader does.
		std::vector<Line> Split(std::string_view a_text)
		{
			std::vector<Line> lines;
			std::size_t       start = 0;
			while (start < a_text.size()) {
				const auto end = a_text.find_first_of("\r\n", start);
				if (end == std::string_view::npos) {
					lines.push_back({ a_text.substr(start), {} });
					break;
				}
				const std::size_t size = a_text.compare(end, 2, "\r\n") == 0 ? 2 : 1;
				lines.push_back({ a_text.substr(start, end - start), a_text.substr(end, size) });
				start = end + size;
			}
			return lines;
		}

		std::string_view Trimmed(std::string_view a_text)
		{
			const auto first = a_text.find_first_not_of(" \t");
			if (first == std::string_view::npos) {
				return {};
			}
			return a_text.substr(first, a_text.find_last_not_of(" \t") - first + 1);
		}

		// Names match in any case, A to Z only, as the reader matches them.
		bool SameName(std::string_view a_lhs, std::string_view a_rhs)
		{
			const auto lower = [](char a_char) {
				return a_char >= 'A' && a_char <= 'Z' ? static_cast<char>(a_char - 'A' + 'a') : a_char;
			};
			if (a_lhs.size() != a_rhs.size()) {
				return false;
			}
			for (std::size_t i = 0; i < a_lhs.size(); i++) {
				if (lower(a_lhs[i]) != lower(a_rhs[i])) {
					return false;
				}
			}
			return true;
		}

		// What the reader makes of one line: a section header, a key, a [
		// with no ] on its line, or nothing, which is a comment, a blank line
		// or a line it skips.
		struct Meaning
		{
			enum class Kind
			{
				kNothing,
				kSection,
				kKey,
				kBroken,  // [Balance with its ] left out
				kOpen,    // a [ with nothing after it
			};

			Kind             kind = Kind::kNothing;
			std::string_view name;
		};

		Meaning Read(std::string_view a_line)
		{
			const auto text = Trimmed(a_line);
			if (text.empty() || text.front() == ';' || text.front() == '#') {
				return {};
			}
			// The reader skips anything after the ].
			if (text.front() == '[') {
				const auto close = text.find(']');
				if (close == std::string_view::npos) {
					return { Trimmed(text.substr(1)).empty() ? Meaning::Kind::kOpen : Meaning::Kind::kBroken, {} };
				}
				return { Meaning::Kind::kSection, Trimmed(text.substr(1, close - 1)) };
			}
			// A line with no = or nothing before it is skipped too.
			const auto equals = text.find('=');
			if (equals == std::string_view::npos || equals == 0) {
				return {};
			}
			return { Meaning::Kind::kKey, Trimmed(text.substr(0, equals)) };
		}

		// Where a key stands in the text: the lines that set it in its section,
		// and the line a new one goes after, the section's last key or its
		// header. Nothing in the cases WithKey names in IniText.h.
		struct Found
		{
			bool                       bom = false;
			std::vector<Line>          lines;
			std::vector<bool>          key;    // true for each line of the key
			bool                       found = false;
			std::optional<std::size_t> after;  // the line a new key goes after
		};

		std::optional<Found> Find(std::string_view a_text, std::string_view a_section, std::string_view a_key)
		{
			// The reader stops at the first NUL byte, so a file saved as UTF-16
			// reads as nothing.
			if (a_text.find('\0') != std::string_view::npos) {
				return std::nullopt;
			}

			Found found;
			auto  body = a_text;
			found.bom = body.starts_with(BOM);
			if (found.bom) {
				body.remove_prefix(BOM.size());
			}
			found.lines = Split(body);
			found.key.assign(found.lines.size(), false);

			// Lines before the first header sit in no section.
			bool inSection = false;
			for (std::size_t i = 0; i < found.lines.size(); i++) {
				const auto meaning = Read(found.lines[i].text);
				if (meaning.kind == Meaning::Kind::kOpen) {
					// The reader takes the section name from the next line with
					// text, even from a key or a header, so nothing is written.
					return std::nullopt;
				}
				if (meaning.kind == Meaning::Kind::kBroken) {
					// The reader files the keys after it under a name that holds
					// a line break, which is never NEC's.
					inSection = false;
				} else if (meaning.kind == Meaning::Kind::kSection) {
					inSection = SameName(meaning.name, a_section);
					if (inSection) {
						found.after = i;
					}
				} else if (meaning.kind == Meaning::Kind::kKey && inSection) {
					found.after = i;
					if (SameName(meaning.name, a_key)) {
						found.key[i] = true;
						found.found = true;
					}
				}
			}
			return found;
		}
	}

	std::optional<std::string> WithKey(std::string_view a_text, std::string_view a_section, std::string_view a_key, std::string_view a_value)
	{
		const auto place = Find(a_text, a_section, a_key);
		if (!place) {
			return std::nullopt;
		}
		const auto& [bom, lines, rewrite, found, after] = *place;

		// New lines end the way the file's first line does.
		std::string_view ending = "\n";
		for (const auto& line : lines) {
			if (!line.ending.empty()) {
				ending = line.ending;
				break;
			}
		}

		std::string entry{ a_key };
		entry += '=';
		entry += a_value;

		std::string out;
		out.reserve(a_text.size() + entry.size() + a_section.size() + 8);
		if (bom) {
			out += BOM;
		}
		for (std::size_t i = 0; i < lines.size(); i++) {
			out += rewrite[i] ? std::string_view{ entry } : lines[i].text;
			if (!found && after == i) {
				out += lines[i].ending.empty() ? ending : lines[i].ending;
				out += entry;
			}
			out += lines[i].ending;
			if (!found && after == i && lines[i].ending.empty()) {
				out += ending;
			}
		}
		if (found || after) {
			return out;
		}

		// No such section, so it goes at the end after a blank line.
		if (!lines.empty()) {
			if (lines.back().ending.empty()) {
				out += ending;
			}
			out += ending;
		}
		out += '[';
		out += a_section;
		out += ']';
		out += ending;
		out += entry;
		out += ending;
		return out;
	}

	std::optional<std::string> WithoutKey(std::string_view a_text, std::string_view a_section, std::string_view a_key)
	{
		const auto place = Find(a_text, a_section, a_key);
		if (!place) {
			return std::nullopt;
		}

		std::string out;
		out.reserve(a_text.size());
		if (place->bom) {
			out += BOM;
		}
		for (std::size_t i = 0; i < place->lines.size(); i++) {
			if (!place->key[i]) {
				out += place->lines[i].text;
				out += place->lines[i].ending;
			}
		}
		return out;
	}
}
