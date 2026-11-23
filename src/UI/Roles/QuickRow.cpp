#include "UI/Roles/QuickRow.h"

#include "UI/Flash.h"

#include <algorithm>
#include <string>

namespace Roles::QuickRow
{
	namespace
	{
		// The field a quick container row draws its name in, so a clip of
		// the list holding one is a row.
		constexpr const char* NAME_FIELD = "ItemName_tf";

		// The text a_field shows, empty for a clip that is no text field, as
		// a HUD mod's icon clip that keeps a text member of its own.
		[[nodiscard]] std::string TextOf(const Value& a_field)
		{
			const auto text = Flash::Member(a_field, "text"sv);
			return text.IsString() && a_field.IsObject() && a_field.HasMember("htmlText") ? text.GetString() : std::string{};
		}

		// Whether a_component shows over a_box, as Clear counts it.
		[[nodiscard]] bool Over(Value& a_component, Value& a_row, const Box& a_box, const Box& a_room)
		{
			if (!a_component.IsDisplayObject() || Flash::Opacity(a_component) <= 0.0 || Flash::Member(a_component, "text"sv).IsString() ||
				Flash::String(a_component, "name"sv).starts_with("NEC_"sv)) {
				return false;
			}
			if (Flash::Number(a_component, "numChildren"sv) > 0.0) {
				for (auto& child : Flash::Children(a_component)) {
					if (Over(child, a_row, a_box, a_room)) {
						return true;
					}
				}
				return false;
			}
			const auto box = BoundsIn(a_component, a_row);
			return box && box->Meets(a_box) && !(box->left <= a_room.left && box->right >= a_room.right);
		}
	}

	bool Clear(Value& a_row, const Box& a_box, const Box& a_room)
	{
		for (auto& component : Flash::Children(a_row)) {
			if (Over(component, a_row, a_box, a_room)) {
				return false;
			}
		}
		auto list = Flash::Member(a_row, "parent"sv);
		for (auto& component : Flash::Children(list)) {
			if (!Flash::Child(component, NAME_FIELD).IsDisplayObject() && Over(component, a_row, a_box, a_room)) {
				return false;
			}
		}
		return true;
	}

	bool OfName(Value& a_row, Value& a_field)
	{
		const auto name = Flash::String(a_field, "name"sv);
		if (name == NAME_FIELD) {
			return true;
		}
		const auto text = TextOf(a_field);
		if (name.starts_with("NEC_"sv) || text.empty()) {
			return false;
		}
		return TextOf(Flash::Child(a_row, NAME_FIELD)).find(text) != std::string::npos;
	}

	std::vector<Value> NameFields(Scaleform::GFx::Movie& a_movie, Value& a_row)
	{
		std::vector<Value> fields;
		for (auto& child : Flash::Children(a_row)) {
			if (Flash::String(child, "name"sv) == NAME_FIELD || !OfName(a_row, child) || !OnScreen(a_movie, child)) {
				continue;
			}
			fields.push_back(child);
		}
		if (!fields.empty()) {
			Found(a_movie, "quick container name drawn beside its own field"sv, "by a walk over the row"sv);
		}
		return fields;
	}

	std::optional<double> ColumnStart(Scaleform::GFx::Movie& a_movie, Value& a_row, double a_from)
	{
		std::optional<double> start;
		for (auto& child : Flash::Children(a_row)) {
			const auto x = Flash::Number(child, "x"sv);
			if (TextOf(child).empty() || Flash::String(child, "name"sv).starts_with("NEC_"sv) || OfName(a_row, child) || !(x > a_from) ||
				!OnScreen(a_movie, child)) {
				continue;
			}
			start = std::min(start.value_or(x), x);
		}
		if (start) {
			Found(a_movie, "quick container column of a HUD mod"sv, "by a walk over the row"sv);
		}
		return start;
	}
}
