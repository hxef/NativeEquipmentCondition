#include "UI/Hud/PickupBar/Names.h"

#include <REX/CONVERT.h>
#include <REX/W32/KERNEL32.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace PickupBar
{
	namespace
	{
		// The text in capitals, each letter raised on its own with no
		// language's rules, as vanilla's prompt raises the name it prints.
		// Text that will not convert stays as it is.
		std::wstring Capitals(std::string_view a_text)
		{
			std::wstring wide;
			if (!REX::UTF8_TO_UTF16(a_text, wide)) {
				return wide;
			}
			const auto   size = static_cast<std::int32_t>(wide.size());
			std::wstring capitals(wide.size(), L'\0');
			const auto   written = REX::W32::LCMapStringEx(REX::W32::LOCALE_NAME_INVARIANT, REX::W32::LCMAP_UPPERCASE, wide.data(), size, capitals.data(), size, nullptr, nullptr, 0);
			return written == size ? capitals : wide;
		}

		// The name without a sorter's tag in front: 1 bracket group that opens
		// it, [..], (..), {..} or |..|, and the spaces after it, so
		// [10mm] 10mm Pistol gives 10mm Pistol. A name with no such group, or
		// with nothing after it, stays as it is.
		std::string_view Untagged(std::string_view a_name)
		{
			constexpr std::string_view OPENS = "[({|";
			constexpr std::string_view CLOSES = "])}|";
			const auto kind = a_name.empty() ? std::string_view::npos : OPENS.find(a_name.front());
			const auto close = kind == std::string_view::npos ? std::string_view::npos : a_name.find(CLOSES[kind], 1);
			const auto rest = close == std::string_view::npos ? std::string_view::npos : a_name.find_first_not_of(' ', close + 1);
			return rest == std::string_view::npos ? a_name : a_name.substr(rest);
		}

		// Whether a_text is a pile's count as vanilla prints it after the
		// name, a space and a number in brackets, such as " (3)".
		bool IsCount(std::wstring_view a_text)
		{
			if (a_text.size() < 4 || !a_text.starts_with(L" (") || !a_text.ends_with(L')')) {
				return false;
			}
			const auto digits = a_text.substr(2, a_text.size() - 3);
			return digits.find_first_not_of(L"0123456789") == std::wstring_view::npos;
		}
	}

	bool Names(std::string_view a_text, std::string_view a_name)
	{
		const auto text = Capitals(a_text);
		if (text.find(Capitals(a_name)) != std::wstring::npos) {
			return true;
		}
		const auto untagged = Untagged(a_name);
		if (untagged.size() == a_name.size()) {
			return false;
		}
		const auto name = Capitals(untagged);
		const auto after = std::wstring_view{ text }.substr(std::min(name.size(), text.size()));
		return !name.empty() && text.starts_with(name) && (after.empty() || IsCount(after));
	}
}
