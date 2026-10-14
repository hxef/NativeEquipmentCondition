#pragma once

#include "Core/Plugin.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Safe readers, writers and calls for a menu movie's objects, their colour,
// boxes and points, a text field's first line, and text written for it. Each
// one that takes a clip checks that it is an object first, so a clip a UI
// replacer removed reads as absent. A number comes back whichever of the 3
// number types the movie keeps it in.
//
// Every native function NEC gives a movie, each listener and handler, is a
// global. Scaleform counts references to it and its count starts at 1,
// which is never given back, so it lives as long as the plugin.
namespace Flash
{
	using Value = Scaleform::GFx::Value;

	// A text field keeps a 2 wide margin inside its box.
	inline constexpr double FIELD_MARGIN = 2.0;

	// A box in some clip's units, from its left edge to its right and from
	// its top down to its bottom.
	struct Box
	{
		double left = 0.0;
		double top = 0.0;
		double right = 0.0;
		double bottom = 0.0;

		bool operator==(const Box&) const = default;

		// Boxes that only touch do not meet.
		[[nodiscard]] constexpr bool Meets(const Box& a_other) const noexcept
		{
			return left < a_other.right && a_other.left < right && top < a_other.bottom && a_other.top < bottom;
		}

		[[nodiscard]] constexpr bool Holds(const Box& a_other) const noexcept
		{
			return left <= a_other.left && a_other.right <= right && top <= a_other.top && a_other.bottom <= bottom;
		}

		[[nodiscard]] constexpr Box Moved(double a_x, double a_y) const noexcept
		{
			return Box{ left + a_x, top + a_y, right + a_x, bottom + a_y };
		}

		// The smallest box holding both.
		[[nodiscard]] constexpr Box With(const Box& a_other) const noexcept
		{
			return Box{ std::min(left, a_other.left), std::min(top, a_other.top), std::max(right, a_other.right), std::max(bottom, a_other.bottom) };
		}
	};

	// A point in some clip's units.
	struct Point
	{
		double x = 0.0;
		double y = 0.0;
	};

	// A value as a number, whichever of the 3 number types it is. 0 for
	// anything else.
	[[nodiscard]] double AsNumber(const Value& a_value);

	// Whether a value is a number, whichever of the 3 number types it is.
	[[nodiscard]] bool IsAnyNumber(const Value& a_value);

	// A member of a clip, or undefined when there is none.
	[[nodiscard]] Value Member(const Value& a_object, std::string_view a_name);

	// A member of a clip as a number, or a_absent when there is none.
	[[nodiscard]] double Number(const Value& a_object, std::string_view a_name, double a_absent = 0.0);

	// A member of a clip as true or false, or false when there is none.
	[[nodiscard]] bool Bool(const Value& a_object, std::string_view a_name);

	// A member of a clip as text, or "(none)" when there is none.
	[[nodiscard]] std::string String(const Value& a_object, std::string_view a_name);

	// How visible a clip is, 0 hidden to 1 shown. The HUD hides parts by fading
	// alpha or switching visible off, so both count.
	[[nodiscard]] double Opacity(const Value& a_clip);

	// Writes a member of a clip. False when there is no clip or the movie
	// refuses the write.
	bool Set(Value& a_object, std::string_view a_name, const Value& a_value);

	// Calls a function of a clip with a_args. False when there is no clip or
	// the call fails.
	bool Call(Value& a_object, const char* a_name, std::span<const Value> a_args = {});

	// A clip's child by name, or a value that is not a display object when
	// there is none.
	[[nodiscard]] Value Child(Value& a_parent, const char* a_name);

	// A clip's child by index from 0, or a value that is not a display object
	// when there is none.
	[[nodiscard]] Value ChildAt(Value& a_parent, std::uint32_t a_index);

	// A clip's children that are display objects, from the bottom up.
	[[nodiscard]] std::vector<Value> Children(Value& a_parent);

	// A clip's colour as its colorTransform keeps it: what red, green and
	// blue are each multiplied by, then what is added to each, out of 255.
	// Alpha is left out, since NEC fades its own parts by alpha.
	struct Tint
	{
		double red = 1.0;
		double green = 1.0;
		double blue = 1.0;
		double redAdd = 0.0;
		double greenAdd = 0.0;
		double blueAdd = 0.0;

		// This colour on a parent, over a_child's own, as the screen shows
		// the child.
		[[nodiscard]] Tint Over(const Tint& a_child) const noexcept;

		// Whether 2 colours look the same: each multiplier within a 500th
		// and each added part within half a step.
		[[nodiscard]] bool Near(const Tint& a_other) const noexcept;
	};

	// A clip's colour, or nothing when it has none to read.
	[[nodiscard]] std::optional<Tint> TintOf(const Value& a_clip);

	// Gives a clip a colour and keeps its alpha. False when there is no clip
	// or the movie refuses a write.
	bool SetTint(Value& a_clip, const Tint& a_tint);

	// A clip's bounds in the stage's units, with x, y, width and height.
	// Nothing for a clip outside the display list, which has no stage.
	[[nodiscard]] std::optional<Value> StageBounds(Value& a_clip);

	// A text field's first line in the field's own units, from its box's top
	// left corner, as getLineMetrics gives it: where its letters start and
	// end across and the top of the line, each inside the margin, its
	// baseline, where capitals and digits end, and how far the line reaches
	// below that. Nothing for a clip with no line to measure.
	struct TextLine
	{
		double left = 0.0;
		double right = 0.0;
		double top = 0.0;
		double baseline = 0.0;
		double descent = 0.0;
	};
	[[nodiscard]] std::optional<TextLine> FirstLine(Value& a_field);

	// a_text with &, < and > written for an htmlText field.
	[[nodiscard]] std::string HtmlEscaped(std::string_view a_text);
}
