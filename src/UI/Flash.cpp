#include "UI/Flash.h"

#include <algorithm>
#include <cmath>

namespace Flash
{
	// -------------------------------------------------------------------
	// Reading a member
	// -------------------------------------------------------------------

	double AsNumber(const Value& a_value)
	{
		switch (a_value.GetType()) {
		case Value::ValueType::kInt:
			return a_value.GetInt();
		case Value::ValueType::kUInt:
			return a_value.GetUInt();
		case Value::ValueType::kNumber:
			return a_value.GetNumber();
		default:
			return 0.0;
		}
	}

	bool IsAnyNumber(const Value& a_value)
	{
		return a_value.IsNumber() || a_value.IsInt() || a_value.IsUInt();
	}

	// The engine reads a value's object without looking, so reading, writing
	// or calling a member of a value that holds no object crashes the game.
	// The asserts that would catch it are compiled out of a release build.
	Value Member(const Value& a_object, std::string_view a_name)
	{
		Value member;
		if (a_object.IsObject()) {
			a_object.GetMember(a_name, &member);
		}
		return member;
	}

	double Number(const Value& a_object, std::string_view a_name, double a_absent)
	{
		Value member;
		return a_object.IsObject() && a_object.GetMember(a_name, &member) ? AsNumber(member) : a_absent;
	}

	bool Bool(const Value& a_object, std::string_view a_name)
	{
		Value member;
		return a_object.IsObject() && a_object.GetMember(a_name, &member) && member.IsBoolean() && member.GetBoolean();
	}

	std::string String(const Value& a_object, std::string_view a_name)
	{
		Value member;
		return a_object.IsObject() && a_object.GetMember(a_name, &member) && member.IsString() ? member.GetString() : "(none)";
	}

	double Opacity(const Value& a_clip)
	{
		Value visible;
		if (a_clip.IsObject() && a_clip.GetMember("visible"sv, &visible) && visible.IsBoolean() && !visible.GetBoolean()) {
			return 0.0;
		}
		return std::clamp(Number(a_clip, "alpha"sv), 0.0, 1.0);
	}

	// -------------------------------------------------------------------
	// Writing a member and calling a function
	// -------------------------------------------------------------------

	bool Set(Value& a_object, std::string_view a_name, const Value& a_value)
	{
		return a_object.IsObject() && a_object.SetMember(a_name, a_value);
	}

	bool Call(Value& a_object, const char* a_name, std::span<const Value> a_args)
	{
		return a_object.IsObject() && a_object.Invoke(a_name, nullptr, a_args.data(), a_args.size());
	}

	Value Child(Value& a_parent, const char* a_name)
	{
		Value child;
		const Value name{ a_name };
		if (a_parent.IsObject()) {
			a_parent.Invoke("getChildByName", &child, &name, 1);
		}
		return child;
	}

	Value ChildAt(Value& a_parent, std::uint32_t a_index)
	{
		Value      child;
		const auto index = Value(static_cast<std::int32_t>(a_index));
		if (a_parent.IsObject()) {
			a_parent.Invoke("getChildAt", &child, &index, 1);
		}
		return child;
	}

	std::vector<Value> Children(Value& a_parent)
	{
		std::vector<Value> children;
		const auto         count = Number(a_parent, "numChildren"sv);
		for (std::uint32_t i = 0; i < count; i++) {
			if (auto child = ChildAt(a_parent, i); child.IsDisplayObject()) {
				children.push_back(child);
			}
		}
		return children;
	}

	// -------------------------------------------------------------------
	// A clip's colour
	// -------------------------------------------------------------------

	Tint Tint::Over(const Tint& a_child) const noexcept
	{
		// The child's colour applies first and this one to the result.
		return Tint{ red * a_child.red, green * a_child.green, blue * a_child.blue,
			red * a_child.redAdd + redAdd, green * a_child.greenAdd + greenAdd, blue * a_child.blueAdd + blueAdd };
	}

	bool Tint::Near(const Tint& a_other) const noexcept
	{
		constexpr double MULTIPLIER_LEEWAY = 1.0 / 500.0;
		constexpr double ADDED_LEEWAY = 0.5;
		return std::abs(red - a_other.red) <= MULTIPLIER_LEEWAY && std::abs(green - a_other.green) <= MULTIPLIER_LEEWAY &&
		       std::abs(blue - a_other.blue) <= MULTIPLIER_LEEWAY && std::abs(redAdd - a_other.redAdd) <= ADDED_LEEWAY &&
		       std::abs(greenAdd - a_other.greenAdd) <= ADDED_LEEWAY && std::abs(blueAdd - a_other.blueAdd) <= ADDED_LEEWAY;
	}

	std::optional<Tint> TintOf(const Value& a_clip)
	{
		const auto colors = Member(Member(a_clip, "transform"sv), "colorTransform"sv);
		if (!colors.IsObject()) {
			return std::nullopt;
		}
		const Tint tint{ Number(colors, "redMultiplier"sv), Number(colors, "greenMultiplier"sv), Number(colors, "blueMultiplier"sv),
			Number(colors, "redOffset"sv), Number(colors, "greenOffset"sv), Number(colors, "blueOffset"sv) };
		const auto parts = { tint.red, tint.green, tint.blue, tint.redAdd, tint.greenAdd, tint.blueAdd };
		if (!std::ranges::all_of(parts, [](double a_part) { return std::isfinite(a_part); })) {
			return std::nullopt;
		}
		return tint;
	}

	bool SetTint(Value& a_clip, const Tint& a_tint)
	{
		// The getter hands back a copy, so the copy is changed and written
		// back whole, with its alpha as it was.
		auto transform = Member(a_clip, "transform"sv);
		auto colors = Member(transform, "colorTransform"sv);
		return Set(colors, "redMultiplier"sv, Value(a_tint.red)) && Set(colors, "greenMultiplier"sv, Value(a_tint.green)) &&
		       Set(colors, "blueMultiplier"sv, Value(a_tint.blue)) && Set(colors, "redOffset"sv, Value(a_tint.redAdd)) &&
		       Set(colors, "greenOffset"sv, Value(a_tint.greenAdd)) && Set(colors, "blueOffset"sv, Value(a_tint.blueAdd)) &&
		       Set(transform, "colorTransform"sv, colors);
	}

	// -------------------------------------------------------------------
	// Where a clip is
	// -------------------------------------------------------------------

	std::optional<Value> StageBounds(Value& a_clip)
	{
		Value stage;
		Value bounds;
		if (!a_clip.IsDisplayObject() || !a_clip.GetMember("stage"sv, &stage) || !stage.IsObject() ||
			!a_clip.Invoke("getBounds", &bounds, &stage, 1) || !bounds.IsObject()) {
			return std::nullopt;
		}
		return bounds;
	}

	std::optional<TextLine> FirstLine(Value& a_field)
	{
		Value       metrics;
		const Value line{ 0 };
		if (!a_field.IsDisplayObject() || !a_field.Invoke("getLineMetrics", &metrics, &line, 1) || !metrics.IsObject()) {
			return std::nullopt;
		}
		// The game reads x as 0 for a line at the left, yet draws its letters
		// past the 2 wide margin, as it does the line's top.
		const auto     left = FIELD_MARGIN + Number(metrics, "x"sv);
		const TextLine first{ left, left + Number(metrics, "width"sv), FIELD_MARGIN, FIELD_MARGIN + Number(metrics, "ascent"sv), Number(metrics, "descent"sv) };
		const auto     parts = { first.left, first.right, first.baseline, first.descent };
		if (!std::ranges::all_of(parts, [](double a_part) { return std::isfinite(a_part); }) || first.right < first.left || !(first.baseline > first.top)) {
			return std::nullopt;
		}
		return first;
	}

	// -------------------------------------------------------------------
	// Text for an htmlText field
	// -------------------------------------------------------------------

	std::string HtmlEscaped(std::string_view a_text)
	{
		std::string escaped;
		for (const auto c : a_text) {
			switch (c) {
			case '&':
				escaped += "&amp;";
				break;
			case '<':
				escaped += "&lt;";
				break;
			case '>':
				escaped += "&gt;";
				break;
			default:
				escaped += c;
				break;
			}
		}
		return escaped;
	}
}
