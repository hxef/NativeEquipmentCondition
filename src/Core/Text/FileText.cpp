#include "Core/Text/FileText.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace Text
{
	namespace
	{
		// 1 bit per slot in a std::uint32_t.
		constexpr std::uint32_t MOST_SLOTS = 32;

		// The 1 based line of a_text that holds byte a_at.
		std::size_t LineAt(std::string_view a_text, std::size_t a_at)
		{
			return 1 + static_cast<std::size_t>(std::ranges::count(a_text.substr(0, a_at), '\n'));
		}

		// The bytes of a UTF-8 letter that starts with a_lead, or 0 for a byte
		// no letter starts with.
		std::size_t SizeOf(unsigned char a_lead)
		{
			if (a_lead < 0x80) {
				return 1;
			}
			if (a_lead >= 0xC2 && a_lead <= 0xDF) {
				return 2;
			}
			if (a_lead >= 0xE0 && a_lead <= 0xEF) {
				return 3;
			}
			return a_lead >= 0xF0 && a_lead <= 0xF4 ? 4 : 0;
		}

		// Where a_text stops being UTF-8, or npos when all of it is. No
		// overlong form, no half of a UTF-16 pair and nothing past U+10FFFF.
		std::size_t BadUtf8(std::string_view a_text)
		{
			for (std::size_t i = 0; i < a_text.size();) {
				const auto byte = static_cast<unsigned char>(a_text[i]);
				const auto size = SizeOf(byte);
				if (size == 0 || i + size > a_text.size()) {
					return i;
				}
				for (std::size_t k = 1; k < size; k++) {
					if ((static_cast<unsigned char>(a_text[i + k]) & 0xC0) != 0x80) {
						return i;
					}
				}
				const auto second = size > 1 ? static_cast<unsigned char>(a_text[i + 1]) : 0;
				if ((byte == 0xE0 && second < 0xA0) || (byte == 0xED && second > 0x9F) ||
					(byte == 0xF0 && second < 0x90) || (byte == 0xF4 && second > 0x8F)) {
					return i;
				}
				i += size;
			}
			return std::string_view::npos;
		}

		// Whether a_wide holds half of a UTF-16 pair without its other half,
		// which the converter would quietly turn into a replacement letter.
		bool HalfPair(std::wstring_view a_wide)
		{
			const auto high = [](wchar_t a_unit) { return a_unit >= 0xD800 && a_unit <= 0xDBFF; };
			const auto low = [](wchar_t a_unit) { return a_unit >= 0xDC00 && a_unit <= 0xDFFF; };
			for (std::size_t i = 0; i < a_wide.size(); i++) {
				if (high(a_wide[i]) && i + 1 < a_wide.size() && low(a_wide[i + 1])) {
					i++;
				} else if (high(a_wide[i]) || low(a_wide[i])) {
					return true;
				}
			}
			return false;
		}

		// The {n} slots of a text, 1 bit per n, or nothing and why not. Only a
		// bare {0} to {31} counts, {{ and }} being a brace.
		std::optional<std::uint32_t> SlotsOf(std::string_view a_text, std::string& a_why)
		{
			std::uint32_t slots = 0;
			for (std::size_t i = 0; i < a_text.size(); i++) {
				if (a_text.substr(i, 2) == "{{" || a_text.substr(i, 2) == "}}") {
					i++;
				} else if (a_text[i] == '}') {
					a_why = "holds a } that closes no {0}, {1} or {2}";
					return std::nullopt;
				} else if (a_text[i] == '{') {
					const auto digits = a_text.find_first_not_of("0123456789", i + 1);
					const auto number = a_text.substr(i + 1, digits == std::string_view::npos ? std::string_view::npos : digits - i - 1);
					std::uint32_t n = 0;
					const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), n);
					const auto bare = !number.empty() && error == std::errc{} && end == number.data() + number.size() && (number == "0" || number.front() != '0');
					if (bare && digits != std::string_view::npos && a_text[digits] == ':') {
						const auto close = a_text.find('}', digits);
						a_why = std::format("holds {:s}, and only a bare {{{:s}}} works", a_text.substr(i, close == std::string_view::npos ? close : close - i + 1), number);
						return std::nullopt;
					}
					if (!bare || digits == std::string_view::npos || a_text[digits] != '}') {
						a_why = "holds a { that is no {0}, {1} or {2}";
						return std::nullopt;
					}
					if (n >= MOST_SLOTS) {
						a_why = std::format("holds {{{:d}}}, which the English line lacks", n);
						return std::nullopt;
					}
					slots |= 1u << n;
					i = digits;
				}
			}
			return slots;
		}
	}

	std::optional<std::string> Decoded(std::string_view a_bytes, std::string& a_why)
	{
		constexpr std::string_view UTF8_MARK = "\xEF\xBB\xBF";
		constexpr std::string_view UTF16_MARK = "\xFF\xFE";
		constexpr std::string_view UTF16_BE_MARK = "\xFE\xFF";
		std::string text;
		if (a_bytes.starts_with(UTF16_MARK)) {
			const auto body = a_bytes.substr(UTF16_MARK.size());
			std::wstring wide(body.size() / 2, L'\0');
			std::memcpy(wide.data(), body.data(), wide.size() * 2);
			if (body.size() % 2 != 0 || HalfPair(wide) || (!wide.empty() && !REX::UTF16_TO_UTF8(wide, text))) {
				a_why = "is cut short or broken UTF-16";
				return std::nullopt;
			}
		} else if (a_bytes.starts_with(UTF16_BE_MARK)) {
			a_why = "is UTF-16 BE, as a file saved as \"Unicode big endian\" is";
			return std::nullopt;
		} else {
			text = a_bytes.starts_with(UTF8_MARK) ? a_bytes.substr(UTF8_MARK.size()) : a_bytes;
		}
		if (text.find('\0') != std::string::npos) {
			a_why = "holds NUL bytes, as a file saved as UTF-16 without a mark does";
			return std::nullopt;
		}
		if (const auto bad = BadUtf8(text); bad != std::string::npos) {
			a_why = std::format("is no UTF-8 at line {:d}, as a file saved as ANSI is", LineAt(text, bad));
			return std::nullopt;
		}
		return text;
	}

	std::optional<std::string> SlotFault(std::string_view a_text, Key a_key)
	{
		std::string why;
		const auto english = SlotsOf(InEnglish(a_key), why).value_or(0);
		const auto slots = SlotsOf(a_text, why);
		if (!slots) {
			return why;
		}
		for (std::uint32_t n = 0; n < MOST_SLOTS; n++) {
			const auto bit = 1u << n;
			if ((english & bit) && !(*slots & bit)) {
				return std::format("lacks {{{:d}}}, which the English line has", n);
			}
			if (!(english & bit) && (*slots & bit)) {
				return std::format("holds {{{:d}}}, which the English line lacks", n);
			}
		}
		return std::nullopt;
	}
}
