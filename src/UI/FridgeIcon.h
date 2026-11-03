#pragma once

#include "Core/Plugin.h"
#include "UI/Flash.h"

#include <optional>

// The fridge icon the quick container's name and a food's card share. It is
// Fallout 4's cryo damage icon, a snowflake, made from the damage icon class
// of a movie the game loads, so a mod that changes the icon changes it here.
namespace FridgeIcon
{
	using Value = Scaleform::GFx::Value;

	// The icon a_parent holds, or a value that is not a display object.
	[[nodiscard]] Value Of(Value& a_parent);

	// Makes the icon from a_class at its cryo frame, as a_parent's hidden
	// child. A value that is not a display object when the movie has no such
	// class or a_parent will not take it.
	[[nodiscard]] Value Make(Scaleform::GFx::Movie& a_movie, Value& a_parent, const char* a_class);

	// The first line of a field where the game draws it, in the units of the
	// field's parent.
	struct Line
	{
		double left = 0.0;           // its first letter's left edge
		double right = 0.0;          // its last letter's right edge
		double capitals = 0.0;       // how high its capitals and digits are
		double middle = 0.0;         // halfway between its top and its bottom, the middle its letters show
		double metricsMiddle = 0.0;  // the middle its line metrics alone give, for the trace
	};

	// a_field's first line, from its line metrics and where the game draws
	// it. Nothing when it cannot be measured.
	[[nodiscard]] std::optional<Line> LineOf(Value& a_field);

	enum class Side
	{
		kLeft,   // left of the text, so a_edge is the icon's right edge
		kRight,  // right of it, so a_edge is its left edge
	};

	// Sizes the icon a_height high, centres it on a_middle and puts its edge
	// nearest the text at a_edge, all in its parent's units. a_box is the
	// icon's own bounds. False when that gives no place.
	bool Put(Value& a_icon, const Flash::Box& a_box, Side a_side, double a_edge, double a_height, double a_middle);
}
