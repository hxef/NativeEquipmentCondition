#include "UI/Flash.h"

#include <algorithm>

namespace Flash
{
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

	// The engine reads a value's object without looking, so asking a value
	// that holds no object for a member crashes the game. The asserts that
	// would catch it are compiled out of a release build.
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
}
