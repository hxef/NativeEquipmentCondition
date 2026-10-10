#include "UI/Roles/Card.h"

#include "UI/Flash.h"
#include "UI/MenuMovies.h"

#include <algorithm>
#include <cctype>
#include <format>

namespace Roles::Card
{
	namespace
	{
		// -------------------------------------------------------------------
		// Where each movie keeps its card
		// -------------------------------------------------------------------

		// Vanilla's places, read off the SWFs.
		constexpr Place PLACES[] = {
			{ "PipboyMenu.swf"sv, "_root.Menu_mc.CurrentPage.ItemCard_mc", false },
			{ "ContainerMenu.swf"sv, "_root.FilterHolder_mc.Menu_mc.ItemCard_mc", true },
			{ "BarterMenu.swf"sv, "_root.FilterHolder_mc.Menu_mc.ItemCard_mc", true },
			{ "ExamineMenu.swf"sv, "_root.BaseInstance.ItemCardList_mc", false },
		};

		// -------------------------------------------------------------------
		// Telling a row
		// -------------------------------------------------------------------

		// Whether a clip holds a placed clip under a_name. Every card entry
		// declares Label_tf and Value_tf, and one it does not place reads as
		// null, so only this test tells the rows apart.
		[[nodiscard]] bool Holds(Value& a_clip, std::string_view a_name)
		{
			return Flash::Member(a_clip, a_name).IsDisplayObject();
		}

		[[nodiscard]] bool IsRow(Value& a_child)
		{
			return a_child.IsDisplayObject() && Holds(a_child, "Label_tf"sv);
		}

		// A damage or resistance row, which sums several numbers.
		[[nodiscard]] bool IsCombined(Value& a_row)
		{
			return Holds(a_row, "EntryHolder_mc"sv);
		}

		[[nodiscard]] std::string_view Trimmed(std::string_view a_text)
		{
			constexpr auto SPACE = " \t\r\n"sv;
			const auto     first = a_text.find_first_not_of(SPACE);
			return first == std::string_view::npos ? std::string_view{} : a_text.substr(first, a_text.find_last_not_of(SPACE) - first + 1);
		}

		// The text a row's field prints, with no spaces at either end, or
		// nothing when the row has no such field.
		[[nodiscard]] std::optional<std::string> Printed(Value& a_row, std::string_view a_field)
		{
			auto  field = Flash::Member(a_row, a_field);
			Value text;
			if (!field.IsDisplayObject() || !field.GetMember("text"sv, &text) || !text.IsString()) {
				return std::nullopt;
			}
			return std::string{ Trimmed(text.GetString()) };
		}

		// Case is ignored, since a card may print its labels in capitals.
		[[nodiscard]] bool SameWord(std::string_view a_left, std::string_view a_right)
		{
			return std::ranges::equal(a_left, a_right, [](unsigned char a_l, unsigned char a_r) {
				return std::tolower(a_l) == std::tolower(a_r);
			});
		}

		// Whether a row prints a_label beside a_number, 91 or 91%.
		[[nodiscard]] bool Prints(Value& a_row, std::string_view a_label, const std::string& a_number)
		{
			const auto label = Printed(a_row, "Label_tf"sv);
			if (!label || !SameWord(*label, a_label)) {
				return false;
			}
			const auto value = Printed(a_row, "Value_tf"sv);
			return value && (*value == a_number || *value == a_number + "%");
		}
	}

	// -------------------------------------------------------------------
	// Finding the card
	// -------------------------------------------------------------------

	const Place* For(std::string_view a_file)
	{
		for (const auto& place : PLACES) {
			if (MenuMovies::IsMovie(a_file, place.movie)) {
				return &place;
			}
		}
		return nullptr;
	}

	// A container or barter screen always has its card, so a miss there is a
	// NEC.log warning. Elsewhere it is a trace line: a Pip-Boy page such as
	// STAT has no card and looks the same as one that keeps it elsewhere, and
	// the bench's lists can hold entries with no card data.
	std::optional<Value> Find(Scaleform::GFx::Movie& a_movie, const Place& a_place)
	{
		Value card;
		if (!a_movie.GetVariable(&card, a_place.path) || !card.IsDisplayObject() || !Flash::Member(card, "InfoObj"sv).IsArray()) {
			if (a_place.always) {
				Missing(a_movie, "item card"sv, "no CND row is raised"sv);
			} else {
				Noted(a_movie, "item card"sv, "no CND row is raised"sv);
			}
			return std::nullopt;
		}
		Found(a_movie, "item card"sv, "at vanilla's place"sv);
		return card;
	}

	std::string FirstRowName(Value& a_card)
	{
		const auto children = Flash::Number(a_card, "numChildren"sv);
		for (std::uint32_t i = 0; i < children; i++) {
			if (auto child = Flash::ChildAt(a_card, i); IsRow(child)) {
				return Flash::String(child, "name"sv);
			}
		}
		return children >= 1.0 ? Flash::String(Flash::ChildAt(a_card, 0), "name"sv) : std::string{};
	}

	// -------------------------------------------------------------------
	// Finding the CND row
	// -------------------------------------------------------------------

	std::optional<Row> ConditionRow(Scaleform::GFx::Movie& a_movie, Value& a_card, std::uint32_t a_index,
		std::string_view a_label, double a_percent, bool a_redrawn)
	{
		const auto children = Flash::Number(a_card, "numChildren"sv);
		const auto label = Trimmed(a_label);
		const auto number = std::format("{:.0f}", a_percent);

		// The data order child first, then every other child, so a second
		// row printing the same is seen too.
		std::optional<Row>           row;
		std::optional<std::uint32_t> at;
		std::uint32_t                matches = 0;

		const auto test = [&](std::uint32_t a_child) {
			auto child = Flash::ChildAt(a_card, a_child);
			if (!IsRow(child) || !Prints(child, label, number) || !OnScreen(a_movie, child)) {
				return;
			}
			if (!row) {
				row = Row{ child, {} };
				at = a_child;
			}
			matches++;
		};
		if (a_index < children) {
			test(a_index);
		}
		for (std::uint32_t i = 0; i < children; i++) {
			if (i != a_index) {
				test(i);
			}
		}

		if (matches > 1) {
			Missing(a_movie, "single CND row"sv, "CND stays where the card draws it, since 2 or more rows print it"sv);
			return std::nullopt;
		}
		if (!row) {
			if (a_redrawn) {
				Missing(a_movie, "CND row"sv, "CND stays where the card draws it"sv);
			}
			return std::nullopt;
		}
		Found(a_movie, "CND row"sv, *at == a_index ? "at vanilla's place"sv : "by its label and value"sv);

		// Every card draws the plain rows, then the combined rows, then the
		// description on top, so CND goes no higher than the last combined
		// row. A row that is not shown, such as a hidden description far
		// above the card, is passed over.
		std::size_t combined = 0;
		for (auto i = *at + 1; i < children; i++) {
			auto child = Flash::ChildAt(a_card, i);
			if (!IsRow(child) || !OnScreen(a_movie, child)) {
				continue;
			}
			if (IsCombined(child)) {
				combined = row->above.size() + 1;
			}
			row->above.push_back(std::move(child));
		}

		// With no combined row the plain rows end at the first row with no
		// number, which is the description.
		auto&      above = row->above;
		const auto end = combined > 0 ? above.begin() + static_cast<std::ptrdiff_t>(combined) :
		                                std::ranges::find_if(above, [](Value& a_above) { return !Holds(a_above, "Value_tf"sv); });
		above.erase(end, above.end());
		return row;
	}
}
