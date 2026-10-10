#include "UI/Flash.h"

#include <algorithm>

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
