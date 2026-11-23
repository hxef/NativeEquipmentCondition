#include "UI/Inventory/ItemCard/Cards.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/WeaponWear/WeaponWear.h"
#include "Core/IniText.h"
#include "Core/TraceLog/TraceLog.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "UI/Flash.h"
#include "UI/Inventory/ItemCard/WearRateForms.h"
#include "UI/Roles/Card.h"

#include <charconv>
#include <cmath>
#include <format>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ItemCard
{
	namespace
	{
		using Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// The text
		// -------------------------------------------------------------------

		// The rate as the row prints it, (x1.3): 1 decimal, 2 below 0.05 and a
		// whole number from 100 up, x0 for an item that never wears and x0.01
		// at the least for one that does. The point is "." in every language,
		// as the card prints its other numbers, and nothing is translated.
		std::string WearRateText(float a_rate)
		{
			if (!(a_rate > 0.0F)) {
				return "(x0)";
			}
			if (a_rate < 0.005F) {
				return "(x0.01)";
			}
			if (a_rate < 0.05F) {
				return std::format("(x{:.2f})", a_rate);
			}
			if (a_rate < 99.95F) {
				return std::format("(x{:.1f})", a_rate);
			}
			return std::format("(x{:.0f})", a_rate);
		}

		// The rate in the cell beside the number, which is narrower: a whole
		// number from 10 up, (x18) for 17.6, (x.01) for 0.01, else as
		// WearRateText writes it.
		std::string CellWearRateText(float a_rate)
		{
			// Below 0.05 the 0 before the point goes, since the narrowest cell,
			// 46 wide at Roboto Condensed 18 in one replacer, holds (x.01) at 39
			// and not (x0.01) at 47.5.
			if (a_rate > 0.0F && a_rate < 0.05F) {
				return "(x" + WearRateText(a_rate).substr(3);  // (x0.01) to (x.01)
			}
			return a_rate < 9.95F ? WearRateText(a_rate) : std::format("(x{:.0f})", a_rate);
		}

		// The number a_text holds, as WearRateText prints it: (x1.2) gives 1.2,
		// (x0.01) 0.01 and (x0) 0.
		float PrintedRate(const std::string& a_text)
		{
			const auto number = std::string_view{ a_text }.substr(2, a_text.size() - 3);  // (x1.2) to 1.2
			float      rate = 0.0F;
			std::from_chars(number.data(), number.data() + number.size(), rate);
			return rate;
		}

		// The sign after a rate printed as a_text: + when the item would wear
		// faster than the item compared, printed as a_comparedText, - when
		// slower, and nothing when both print the same.
		std::string_view Sign(const std::string& a_text, const std::string& a_comparedText, float a_rate, float a_compared)
		{
			if (a_text == a_comparedText) {
				return {};
			}
			return a_rate > a_compared ? "+"sv : "-"sv;
		}

		// The texts the cell beside the number may show for a_rate, best
		// first, each also without its brackets, x1.3 + and x262. One
		// replacer's cell holds a text 46 wide, too little for (x1.3) + at
		// size 18 and for (x262) at the Pip-Boy's 21. So the brackets go
		// before the sign does, and only where the text has no room with
		// them. A rate of 1000 or more still has no room at 21. a_compared
		// signs the rate, so it is given only while a mod changes the
		// printed rate.
		std::vector<std::string> CellTries(float a_rate, std::optional<float> a_compared)
		{
			auto       text = CellWearRateText(a_rate);
			const auto sign = a_compared ? Sign(text, CellWearRateText(*a_compared), a_rate, *a_compared) : ""sv;
			const auto bare = text.substr(1, text.size() - 2);
			if (sign.empty()) {
				return { text, bare };
			}
			return { std::format("{:s} {:s}", text, sign), std::format("{:s} {:s}", bare, sign), text, bare };
		}

		// Signs a change the movie printed in a_cell a_way, + or -, the way
		// the rate moves. a_movie names the movie for the trace.
		void TurnChange(std::string_view a_movie, Value& a_cell, char a_way)
		{
			if (const auto turned = TurnedChange(Flash::String(a_cell, "text"sv), a_way); turned && WriteKeepingLook(a_cell, *turned)) {
				TraceLog::First("menu", "{:s} CND row's change beside the number reads \"{:s}\"", a_movie, IniText::Trimmed(*turned));
			}
		}

		// The change the hook fed the CND entry while the 2 rates print apart,
		// by the same test on the same 2 rates, else 0.
		float FedChange(float a_rate, std::optional<float> a_compared)
		{
			return a_compared ? WearRateChange(*a_compared, a_rate) : 0.0F;
		}

		// A movie that prints the fed change beside the number shows its sign
		// as fed, - for a rate that rose, since it turns only Weight's. The
		// rate's way is the other, or nothing with no change.
		std::optional<char> RateWay(float a_change)
		{
			if (a_change == 0.0F) {
				return std::nullopt;
			}
			return a_change < 0.0F ? '+' : '-';
		}

		// -------------------------------------------------------------------
		// What the last write left
		// -------------------------------------------------------------------

		// A CND row as a render finds it, for the rate it should show.
		struct RowState
		{
			const Scaleform::GFx::Movie* movie = nullptr;  // only compared, never read
			std::string                  rate;             // the rate the field should show, (x1.3)
			std::string                  number;           // the number field's text
			std::string                  beside;           // the cell's text, trimmed
			double                       room = 0.0;       // the cell's width, the room WriteInCell measures
			double                       x = 0.0;          // the number field's place and width
			double                       width = 0.0;

			bool operator==(const RowState&) const = default;
		};

		RowState StateOf(const Scaleform::GFx::Movie& a_movie, const Value& a_field, const Value& a_cell, const std::string& a_rate)
		{
			return { &a_movie, a_rate, Flash::String(a_field, "text"sv),
				a_cell.IsDisplayObject() ? std::string{ IniText::Trimmed(Flash::String(a_cell, "text"sv)) } : std::string{},
				Flash::Number(a_cell, "width"sv), Flash::Number(a_field, "x"sv), Flash::Number(a_field, "width"sv) };
		}

		// The row as the last write that measured left it. Only 1 card's
		// menu is open at a time, so 1 is enough, and a row it does not
		// match is only measured again. WatchCard clears it when a card's
		// movie loads, so it never outlives its movie. Behind a lock, as
		// Raise.cpp keeps its own.
		std::mutex g_lastLock;
		RowState   g_last;
	}

	void ForgetWearRate()
	{
		const std::scoped_lock lock{ g_lastLock };
		g_last = {};
	}

	// -------------------------------------------------------------------
	// The rate of an item
	// -------------------------------------------------------------------

	std::optional<float> WearRateOf(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack)
	{
		const auto* object = a_item.object;
		if (!object) {
			return std::nullopt;
		}
		const auto*          extra = a_stack ? a_stack->extra.get() : nullptr;
		std::optional<float> rate;
		if (const auto* weapon = object->As<RE::TESObjectWEAP>()) {
			rate = WeaponWear::WearShare(*weapon, extra);
		} else if (const auto* armor = object->As<RE::TESObjectARMO>()) {
			rate = ArmorWear::WearShare(*armor, extra);
		} else {
			rate = FoodSpoil::SpoilShare(*object);
		}
		return rate && std::isfinite(*rate) ? rate : std::nullopt;
	}

	// UI mods cut a change after 1 decimal, so -0.1 goes as -0.105, which
	// they print as -0.1 however the float rounds it. 2 printed rates 0.1
	// apart can differ by a hair less in a float, 0.09999, which would print
	// as 0.0 and draw no vanilla mark. A change under 0.1 (rates under
	// x0.05) draws no vanilla mark at precision 1, so it is not fed.
	// 1.0628 and 1.1532 give -0.105, 1.0628 and 0.8622 give 0.205, and 5.6793
	// and 5.7007 (both x5.7) give 0.
	float WearRateChange(float a_own, float a_rate)
	{
		const auto change = PrintedRate(WearRateText(a_own)) - PrintedRate(WearRateText(a_rate));
		if (std::abs(change) < 0.095F) {
			return 0.0F;
		}
		return change + std::copysign(0.005F, change);
	}

	// -------------------------------------------------------------------
	// Writing it
	// -------------------------------------------------------------------

	// A render after a write finds the rate it wrote in the field and leaves
	// it. The rate in the cell, or no rate at all, is left only while the row
	// reads as the last write left it for the same rate: the same card, the
	// same number, the same cell text
	// and width, and the field at the same place and width. So a row with
	// no room is measured again when its number, its cell or its box
	// changes, or its movie loads again. A change elsewhere on the row alone,
	// such as the label's size, is not looked for. A rate written for
	// another copy at an earlier render is written again. Each trace line
	// comes once for the same card, since the listener runs at every redraw.
	void WriteWearRate(Scaleform::GFx::Movie& a_movie, std::string_view a_movieFile, Value& a_row, std::optional<float> a_rate,
		std::optional<float> a_compared, bool a_movieCompares)
	{
		if (!a_rate) {
			Roles::Noted(a_movie, "CND row's wear rate"sv, "CND shows alone"sv);
			return;
		}
		auto  field = Roles::Card::NumberField(a_row);
		Value shown;
		if (!field.IsDisplayObject() || !field.GetMember("text", &shown) || !shown.IsString()) {
			return;
		}

		const auto  rate = WearRateText(*a_rate);
		std::string printed{ shown.GetString() };
		const auto  at = printed.find(WEAR_RATE_START);

		const auto fed = FedChange(*a_rate, a_compared);
		const auto way = RateWay(fed);
		const auto movie = MovieName(a_movieFile);
		// Signs a change the movie printed in a_cell the rate's way. It runs
		// at every render while the rate stays in the field too, since the
		// movie may print its change again with the sign as fed.
		const auto turnChange = [&](Value& a_cell) {
			TurnChange(movie, a_cell, *way);
		};

		if (at != std::string::npos && std::string_view{ printed }.substr(at + 1) == rate) {
			if (way) {
				auto cell = Roles::Card::CellBeside(a_row);
				if (ShowsFedChange(Flash::String(cell, "text"sv), fed)) {
					turnChange(cell);
				}
			}
			return;
		}
		auto cell = Roles::Card::CellBeside(a_row);
		const auto now = StateOf(a_movie, field, cell, rate);
		// A cell with a figure of the movie's own is no change NEC fed, so
		// the row reads as if nothing were compared.
		const auto changed = way && ShowsFedChange(now.beside, fed);
		{
			const std::scoped_lock lock{ g_lastLock };
			if (now == g_last) {
				return;
			}
		}
		const auto remember = [&] {
			const std::scoped_lock lock{ g_lastLock };
			g_last = StateOf(a_movie, field, cell, rate);
		};

		// The field's old rate is cut off before the new one goes in.
		if (at != std::string::npos) {
			printed.resize(at);
			Flash::Set(field, "text"sv, Value(printed.c_str()));
		}

		// A cell beside that shows something of its own, a difference to the
		// item compared, leaves no room for the rate, but for the change the
		// hook fed, which the rate may write over. A cell of spaces shows
		// nothing and is left as it is, as on one replacer's workbench card.
		const auto& beside = now.beside;
		const auto cellTries = CellTries(*a_rate, changed ? a_compared : std::nullopt);
		if (beside == cellTries.front()) {
			return;
		}
		if (!beside.empty() && !IsWearRateText(beside) && !changed) {
			TraceLog::First("menu", "{:s} CND row has no room for \"{:s}\" and reads \"{:s}\"", movie, rate, printed);
			return;
		}
		// Once the rate has its place, or none: a rate of an earlier write
		// goes from the cell, and a change the movie printed there is signed
		// the rate's way.
		const auto tidyCell = [&] {
			if (IsWearRateText(beside)) {
				Flash::Set(cell, "text"sv, Value(""));
				return;
			}
			if (changed) {
				turnChange(cell);
			}
		};

		if (const auto text = printed + " " + rate; const auto wider = WriteInField(a_movie, a_row, field, printed, text)) {
			tidyCell();
			TraceLog::First("menu", "{:s} CND row reads \"{:s}\", {:.0f} wider to the left", movie, text, *wider);
			remember();
			return;
		}
		if (cell.IsDisplayObject() && !a_movieCompares) {
			// Only the last try's reason is traced, the rate with no sign and
			// no brackets, as it says why the row shows no rate at all.
			const auto  keepColour = changed && !beside.empty();
			std::string why;
			for (const auto& tried : cellTries) {
				if (WriteInCell(a_movie, field, cell, tried, keepColour, why)) {
					TraceLog::First("menu", "{:s} CND row reads \"{:s}\" with \"{:s}\" in the cell beside it", movie, printed, tried);
					remember();
					return;
				}
			}
			TraceLog::First("menu", "{:s} CND row's cell beside the number {:s}", movie, why);
		}
		tidyCell();
		remember();
		TraceLog::First("menu", "{:s} CND row has no room for \"{:s}\" and reads \"{:s}\"", movie, rate, printed);
	}

	void TurnWearRateChange(std::string_view a_movieFile, Value& a_row, std::optional<float> a_rate, std::optional<float> a_compared)
	{
		const auto fed = a_rate ? FedChange(*a_rate, a_compared) : 0.0F;
		const auto way = RateWay(fed);
		if (!way) {
			return;
		}
		auto cell = Roles::Card::CellBeside(a_row);
		if (ShowsFedChange(Flash::String(cell, "text"sv), fed)) {
			TurnChange(MovieName(a_movieFile), cell, *way);
		}
	}
}
