#include "Core/Text/Text.h"

#include "Core/Text/Lines.h"
#include "Core/TraceLog.h"

#include <REX/CONVERT.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

// What a trader says about a repair and the sign under their caps, the
// [Trader] lines of the language files.
namespace Text
{
	namespace
	{
		// The barter screen's word for caps while a mod renamed it, as the 3
		// lines print it, else empty. See ReadCurrency.
		std::string g_currency;

		// a_text in lower or upper case by a_case. The invariant locale with
		// linguistic casing is right for Latin and Cyrillic letters and leaves
		// Chinese and Japanese as they are.
		[[nodiscard]] std::wstring Cased(std::wstring_view a_text, std::uint32_t a_case)
		{
			const auto flags = a_case | REX::W32::LCMAP_LINGUISTIC_CASING;
			const auto map = [&](wchar_t* a_out, std::int32_t a_size) {
				return REX::W32::LCMapStringEx(REX::W32::LOCALE_NAME_INVARIANT, flags, a_text.data(),
					static_cast<std::int32_t>(a_text.size()), a_out, a_size, nullptr, nullptr, 0);
			};
			std::wstring out(static_cast<std::size_t>(std::max(map(nullptr, 0), 0)), L'\0');
			out.resize(static_cast<std::size_t>(std::max(map(out.data(), static_cast<std::int32_t>(out.size())), 0)));
			return out;
		}

		// Whether this language writes caps with a capital first letter, as
		// German writes every noun. Its own price line tells, "{0} Kronkorken".
		[[nodiscard]] bool Capitalized()
		{
			std::string_view line = Pick(REPAIR_CAPS);
			if (const auto slot = line.find("{0}"); slot != std::string_view::npos) {
				line.remove_prefix(slot + 3);
			}
			line.remove_prefix(std::min(line.find_first_not_of(' '), line.size()));
			std::wstring noun;
			if (!REX::UTF8_TO_UTF16(line, noun) || noun.empty()) {
				return false;
			}
			const std::wstring_view first(noun.data(), 1);
			return Cased(first, REX::W32::LCMAP_LOWERCASE) != first;
		}

		// The line of the 3 for a_trade.
		[[nodiscard]] Key Of(Trade a_trade, Key a_weapons, Key a_armor, Key a_clothing)
		{
			switch (a_trade) {
			case Trade::kArmor:
				return a_armor;
			case Trade::kClothing:
				return a_clothing;
			default:
				return a_weapons;
			}
		}
	}

	// A text field the screen never shows is given "$CAPS", which the movie
	// translates as it does the barter screen's own labels, so the word is
	// whatever the game's translations hold. A key the translations lack
	// stays "$CAPS". Only a word that differs from the language file's
	// CURRENCY_VANILLA counts as renamed, so with vanilla's word every line
	// reads as before.
	void ReadCurrency(Scaleform::GFx::Movie& a_movie)
	{
		g_currency.clear();
		Scaleform::GFx::Value field;
		a_movie.CreateObject(&field, "flash.text.TextField");
		Scaleform::GFx::Value text;
		if (!field.IsDisplayObject() || !field.SetMember("text"sv, Scaleform::GFx::Value("$CAPS")) ||
			!field.GetMember("text"sv, &text) || !text.IsString()) {
			return;
		}
		const std::string word = text.GetString();
		std::wstring live;
		if (word.empty() || word.starts_with('$') || word == Pick(CURRENCY_VANILLA) || !REX::UTF8_TO_UTF16(word, live)) {
			return;
		}
		auto cased = Cased(live, REX::W32::LCMAP_LOWERCASE);
		if (!cased.empty() && Capitalized()) {
			cased.replace(0, 1, Cased(std::wstring_view(cased.data(), 1), REX::W32::LCMAP_UPPERCASE));
		}
		if (cased.empty() || !REX::UTF16_TO_UTF8(cased, g_currency)) {
			g_currency.clear();
			return;
		}
		TraceLog::Line("menu", "The barter screen calls caps {:s}, so the trader's lines say {:s}", word, g_currency);
	}

	void ForgetCurrency()
	{
		g_currency.clear();
	}

	std::string RepairCaps(std::uint32_t a_caps)
	{
		return g_currency.empty() ? Say(REPAIR_CAPS, a_caps) : Say(REPAIR_CAPS_RENAMED, a_caps, g_currency);
	}

	std::string RepairPaid(std::uint32_t a_level, std::uint32_t a_caps)
	{
		return g_currency.empty() ? Say(REPAIR_PAID, a_level, a_caps) : Say(REPAIR_PAID_RENAMED, a_level, a_caps, g_currency);
	}

	std::string RepairUpTo(std::uint32_t a_ceiling, Trade a_trade)
	{
		if (a_ceiling >= FULL) {
			return Pick(Of(a_trade, REPAIR_UP_TO_FULL_WEAPONS, REPAIR_UP_TO_FULL_ARMOR, REPAIR_UP_TO_FULL_CLOTHING));
		}
		return Say(Of(a_trade, REPAIR_UP_TO_WEAPONS, REPAIR_UP_TO_ARMOR, REPAIR_UP_TO_CLOTHING), a_ceiling);
	}

	std::string RepairCeiling(std::uint32_t a_ceiling, Trade a_trade)
	{
		return Say(Of(a_trade, REPAIR_CEILING_WEAPONS, REPAIR_CEILING_ARMOR, REPAIR_CEILING_CLOTHING), a_ceiling);
	}

	std::string RepairUnaffordable()
	{
		return g_currency.empty() ? std::string(Pick(REPAIR_UNAFFORDABLE)) : Say(REPAIR_UNAFFORDABLE_RENAMED, g_currency);
	}

	std::string RepairTradePending()
	{
		return Pick(REPAIR_TRADE_PENDING);
	}

	std::string SignTitle()
	{
		return Pick(TRADER_SIGN_TITLE);
	}

	std::string SignNone()
	{
		return Pick(TRADER_SIGN_NONE);
	}

	std::string SignKind(Trade a_trade)
	{
		return Pick(Of(a_trade, TRADER_SIGN_WEAPONS, TRADER_SIGN_ARMOR, TRADER_SIGN_CLOTHING));
	}
}
