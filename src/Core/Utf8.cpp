#include "Core/Utf8.h"

#include <cstddef>
#include <string_view>

namespace Utf8
{
	namespace
	{
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
	}

	std::size_t LetterSize(std::string_view a_text)
	{
		if (a_text.empty()) {
			return 0;
		}
		const auto lead = static_cast<unsigned char>(a_text[0]);
		const auto size = SizeOf(lead);
		if (size == 0 || size > a_text.size()) {
			return 0;
		}
		for (std::size_t k = 1; k < size; k++) {
			if ((static_cast<unsigned char>(a_text[k]) & 0xC0) != 0x80) {
				return 0;
			}
		}
		const auto second = size > 1 ? static_cast<unsigned char>(a_text[1]) : 0;
		if ((lead == 0xE0 && second < 0xA0) || (lead == 0xED && second > 0x9F) ||
			(lead == 0xF0 && second < 0x90) || (lead == 0xF4 && second > 0x8F)) {
			return 0;
		}
		return size;
	}

	std::size_t BadAt(std::string_view a_text)
	{
		for (std::size_t i = 0; i < a_text.size();) {
			const auto size = LetterSize(a_text.substr(i));
			if (size == 0) {
				return i;
			}
			i += size;
		}
		return std::string_view::npos;
	}

	char32_t CodePoint(std::string_view a_letter)
	{
		// A lead byte keeps 7 bits alone, 5 before 1 more byte, 4 before
		// 2 and 3 before 3. Each byte after it adds 6.
		const auto first = static_cast<char32_t>(static_cast<unsigned char>(a_letter[0]));
		char32_t point = a_letter.size() == 1 ? first : first & (char32_t{ 0x7F } >> a_letter.size());
		for (std::size_t i = 1; i < a_letter.size(); ++i) {
			point = (point << 6) | (static_cast<char32_t>(static_cast<unsigned char>(a_letter[i])) & char32_t{ 0x3F });
		}
		return point;
	}

	bool IsControl(std::string_view a_letter)
	{
		const auto point = CodePoint(a_letter);
		return point < 0x20 || (point >= 0x7F && point < 0xA0);
	}
}
