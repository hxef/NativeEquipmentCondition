#include "UI/Flash.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>

namespace Flash
{
	namespace
	{
		[[nodiscard]] bool AllFinite(std::initializer_list<double> a_values)
		{
			return std::ranges::all_of(a_values, [](double a_value) { return std::isfinite(a_value); });
		}
	}

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

	Value Domain(Scaleform::GFx::Movie& a_movie)
	{
		Value info;
		return a_movie.GetVariable(&info, "_root.loaderInfo") ? Member(info, "applicationDomain"sv) : Value{};
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

	std::uint32_t Tint::OnWhite() const noexcept
	{
		const auto part = [](double a_multiplier, double a_add) {
			return static_cast<std::uint32_t>(std::clamp(std::round(a_multiplier * 255.0 + a_add), 0.0, 255.0));
		};
		return part(red, redAdd) << 16 | part(green, greenAdd) << 8 | part(blue, blueAdd);
	}

	std::optional<Tint> TintOf(const Value& a_clip)
	{
		const auto colors = Member(Member(a_clip, "transform"sv), "colorTransform"sv);
		if (!colors.IsObject()) {
			return std::nullopt;
		}
		const Tint tint{ Number(colors, "redMultiplier"sv), Number(colors, "greenMultiplier"sv), Number(colors, "blueMultiplier"sv),
			Number(colors, "redOffset"sv), Number(colors, "greenOffset"sv), Number(colors, "blueOffset"sv) };
		if (!AllFinite({ tint.red, tint.green, tint.blue, tint.redAdd, tint.greenAdd, tint.blueAdd })) {
			return std::nullopt;
		}
		return tint;
	}

	Tint LetterTint(const Value& a_field)
	{
		// A text field shows white unless it is given a colour. Its
		// textColor scales white down to that colour, the way a tint's
		// multipliers do.
		constexpr std::uint32_t WHITE = 0xFFFFFF;

		const auto color = static_cast<std::uint32_t>(Number(a_field, "textColor"sv, WHITE)) & WHITE;
		const auto part = [&](int a_shift) { return static_cast<double>((color >> a_shift) & 0xFF) / 255.0; };
		return TintOf(a_field).value_or(Tint{}).Over(Tint{ part(16), part(8), part(0) });
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

	std::vector<KeptTint> TextTints(Value& a_parent)
	{
		std::vector<KeptTint> kept;
		for (auto& child : Children(a_parent)) {
			const auto tint = Opacity(child) > 0.0 && Member(child, "text"sv).IsString() ? TintOf(child) : std::nullopt;
			if (tint) {
				kept.push_back({ child, *tint });
			}
		}
		return kept;
	}

	std::size_t PutBackTints(std::span<KeptTint> a_kept)
	{
		std::size_t back = 0;
		for (auto& [field, tint] : a_kept) {
			const auto now = TintOf(field);
			if ((!now || !now->Near(tint)) && SetTint(field, tint)) {
				back++;
			}
		}
		return back;
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
		if (!AllFinite({ first.left, first.right, first.baseline, first.descent }) || first.right < first.left || !(first.baseline > first.top)) {
			return std::nullopt;
		}
		return first;
	}

	// -------------------------------------------------------------------
	// Where a text field draws its first line
	// -------------------------------------------------------------------

	namespace
	{
		// How close the probe for a line's top gets to it.
		constexpr double TOP_TOLERANCE = 0.1;

		// Whether a_field draws a letter at a_x, a_y in its own units, as
		// getCharIndexAtPoint finds one. It gives -1 off every line. Nothing
		// when the call fails.
		[[nodiscard]] std::optional<bool> OnLine(Value& a_field, double a_x, double a_y)
		{
			Value            index;
			const std::array point{ Value(a_x), Value(a_y) };
			if (!a_field.Invoke("getCharIndexAtPoint", &index, point.data(), point.size()) || !IsAnyNumber(index)) {
				return std::nullopt;
			}
			return AsNumber(index) >= 0.0;
		}

		// The top of a_field's first line where the game draws it, in the
		// field's own units, probed at a_x inside the line: down from the
		// box's top in steps of 1 to the first letter, then the last step
		// halved until it is under TOP_TOLERANCE, so the y it gives is on the
		// line and less than that below its top. About 10 calls on a line
		// near the top. Nothing when the box's top already finds a letter,
		// no y finds one or a call fails.
		[[nodiscard]] std::optional<double> DrawnTop(Value& a_field, double a_x)
		{
			const auto height = Number(a_field, "height"sv) / Number(a_field, "scaleY"sv, 1.0);
			const auto atTop = OnLine(a_field, a_x, 0.0);
			if (!std::isfinite(height) || !atTop || *atTop) {
				return std::nullopt;
			}
			auto                  above = 0.0;
			std::optional<double> below;
			for (auto y = 1; !below && y < height; ++y) {
				const auto letter = OnLine(a_field, a_x, y);
				if (!letter) {
					return std::nullopt;
				}
				if (*letter) {
					below = y;
				} else {
					above = y;
				}
			}
			while (below && *below - above >= TOP_TOLERANCE) {
				const auto y = (above + *below) / 2.0;
				const auto letter = OnLine(a_field, a_x, y);
				if (!letter) {
					return std::nullopt;
				}
				if (*letter) {
					below = y;
				} else {
					above = y;
				}
			}
			return below;
		}
	}

	std::optional<TextLine> DrawnLine(Value& a_field)
	{
		auto line = FirstLine(a_field);
		// No property gives the drop, and getCharIndexAtPoint is the one
		// reader that counts it, so a probe down the line's middle finds the
		// drawn top. Where the probe finds none the line stays at the margin.
		const auto top = line ? DrawnTop(a_field, (line->left + line->right) / 2.0) : std::nullopt;
		if (top) {
			line->drop = *top - FIELD_MARGIN;
			line->top += line->drop;
			line->baseline += line->drop;
		}
		return line;
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
