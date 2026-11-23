#include "UI/Roles/Card.h"

#include "Condition/Condition.h"
#include "UI/Flash.h"
#include "UI/MenuMovies.h"

#include <algorithm>
#include <cctype>
#include <string>

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

		// Whether a_value is a_number, 91, 91% or 91% (x1.3) once NEC wrote
		// its wear rate there.
		[[nodiscard]] bool Reads(const std::string& a_value, std::string_view a_number)
		{
			const std::string number{ a_number };
			return a_value == number || a_value == number + "%" || a_value.starts_with(number + "% (x");
		}

		// Whether a row prints a_label beside a_number as Reads takes it. A
		// number with a tenth of 0, such as 98.0, also matches its whole form,
		// 98, since a card may drop the ".0" by itself.
		[[nodiscard]] bool Prints(Value& a_row, std::string_view a_label, std::string_view a_number)
		{
			const auto label = Printed(a_row, "Label_tf"sv);
			if (!label || !SameWord(*label, a_label)) {
				return false;
			}
			const auto value = Printed(a_row, "Value_tf"sv);
			if (!value) {
				return false;
			}
			constexpr auto NO_TENTH = ".0"sv;
			return Reads(*value, a_number) ||
			       (a_number.ends_with(NO_TENTH) && Reads(*value, a_number.substr(0, a_number.size() - NO_TENTH.size())));
		}

		// Whether a row prints a_label and shows a_drawing, the clip NEC drew
		// in place of its number.
		[[nodiscard]] bool Draws(Value& a_row, std::string_view a_label, const char* a_drawing)
		{
			const auto label = Printed(a_row, "Label_tf"sv);
			if (!label || !SameWord(*label, a_label)) {
				return false;
			}
			const auto drawing = Flash::Child(a_row, a_drawing);
			return drawing.IsDisplayObject() && Flash::Opacity(drawing) > 0.0;
		}

		// -------------------------------------------------------------------
		// A row's other children
		// -------------------------------------------------------------------

		// A cell's left edge may touch the number's right edge.
		constexpr double TOUCH = 1.0;

		// Whether a child of a row is one of the 2 fields every entry declares.
		[[nodiscard]] bool IsOwnField(const Value& a_child)
		{
			const auto name = Flash::String(a_child, "name"sv);
			return name == "Label_tf"sv || name == "Value_tf"sv;
		}

		// Every part NEC adds to a row is named NEC_..., the fridge icon and
		// the CND bar, and none takes room from the row's text.
		[[nodiscard]] bool IsNecPart(const Value& a_child)
		{
			return Flash::String(a_child, "name"sv).starts_with("NEC_"sv);
		}

		// Whether a child is a text field, by the text it holds.
		[[nodiscard]] bool IsTextField(const Value& a_child)
		{
			return Flash::Member(a_child, "text"sv).IsString();
		}

		[[nodiscard]] bool PrintsNothing(const Value& a_child)
		{
			const auto text = Flash::Member(a_child, "text"sv);
			return text.IsString() && Trimmed(text.GetString()).empty();
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

	// a_card itself is told by its place in the parent, since 2 values of 1
	// clip cannot be compared.
	std::vector<Value> Beside(Scaleform::GFx::Movie& a_movie, Value& a_card)
	{
		std::vector<Value> cards;
		auto               parent = Flash::Member(a_card, "parent"sv);
		Value              own;
		if (!parent.IsDisplayObject() || !parent.Invoke("getChildIndex", &own, &a_card, 1) || !Flash::IsAnyNumber(own)) {
			return cards;
		}
		const auto children = Flash::Number(parent, "numChildren"sv);
		for (std::uint32_t i = 0; i < children; i++) {
			if (static_cast<double>(i) == Flash::AsNumber(own)) {
				continue;
			}
			auto child = Flash::ChildAt(parent, i);
			if (child.IsDisplayObject() && Flash::Member(child, "InfoObj"sv).IsArray() && OnScreen(a_movie, child)) {
				cards.push_back(std::move(child));
			}
		}
		return cards;
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
		std::string_view a_label, std::uint32_t a_tenths, bool a_redrawn, const char* a_drawing)
	{
		const auto children = Flash::Number(a_card, "numChildren"sv);
		const auto label = Trimmed(a_label);
		const auto number = Condition::CardText(a_tenths);

		// The data order child first, then every other child, so a second
		// row printing the same is seen too.
		std::optional<Row>           row;
		std::optional<std::uint32_t> at;
		std::uint32_t                matches = 0;

		const auto test = [&](std::uint32_t a_child) {
			auto child = Flash::ChildAt(a_card, a_child);
			if (!IsRow(child)) {
				return;
			}
			// A row printing its number again is drawn anew, whatever it
			// showed before.
			const auto prints = Prints(child, label, number);
			if ((!prints && !Draws(child, label, a_drawing)) || !OnScreen(a_movie, child)) {
				return;
			}
			if (!row) {
				row = Row{ child, {}, !prints };
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

	void HideOnOtherRows(Value& a_card, std::string_view a_label, const char* a_drawing)
	{
		const auto label = Trimmed(a_label);
		for (auto& child : Flash::Children(a_card)) {
			if (!IsRow(child)) {
				continue;
			}
			if (const auto printed = Printed(child, "Label_tf"sv); printed && SameWord(*printed, label)) {
				continue;
			}
			if (auto drawing = Flash::Child(child, a_drawing); drawing.IsDisplayObject()) {
				Flash::Set(drawing, "visible"sv, Value(false));
			}
		}
	}

	Value NumberField(Value& a_row)
	{
		auto field = Flash::Member(a_row, "Value_tf"sv);
		return field.IsDisplayObject() ? field : Value{};
	}

	FieldNames RowFieldNames()
	{
		return { "Label_tf", "Value_tf" };
	}

	Value LabelField(Value& a_row)
	{
		auto field = Flash::Member(a_row, "Label_tf"sv);
		return field.IsDisplayObject() ? field : Value{};
	}

	// -------------------------------------------------------------------
	// Room on the row
	// -------------------------------------------------------------------

	bool DrawsAcross(Value& a_row, double a_left, double a_right)
	{
		auto       label = LabelField(a_row);
		auto       number = NumberField(a_row);
		const auto labelBox = label.IsDisplayObject() ? BoundsIn(label, a_row) : std::nullopt;
		const auto numberBox = number.IsDisplayObject() ? BoundsIn(number, a_row) : std::nullopt;
		if (!labelBox || !numberBox) {
			return true;
		}
		for (auto& child : Flash::Children(a_row)) {
			if (IsOwnField(child) || IsNecPart(child) || Flash::Opacity(child) <= 0.0 || PrintsNothing(child)) {
				continue;
			}
			// A child with no size draws nothing.
			const auto box = BoundsIn(child, a_row);
			if (!box) {
				continue;
			}
			const auto background = box->left <= labelBox->left && box->right >= numberBox->right;
			if (!background && box->right > a_left && box->left < a_right) {
				return true;
			}
		}
		return false;
	}

	Value CellBeside(Value& a_row)
	{
		auto       number = NumberField(a_row);
		const auto numberBox = number.IsDisplayObject() ? BoundsIn(number, a_row) : std::nullopt;
		if (!numberBox) {
			return {};
		}
		Value  nearest;
		double nearestLeft = 0.0;
		for (auto& child : Flash::Children(a_row)) {
			if (IsOwnField(child) || !IsTextField(child) || Flash::Opacity(child) <= 0.0) {
				continue;
			}
			const auto box = BoundsIn(child, a_row);
			if (box && box->left >= numberBox->right - TOUCH && (!nearest.IsDisplayObject() || box->left < nearestLeft)) {
				nearest = child;
				nearestLeft = box->left;
			}
		}
		return nearest;
	}
}
