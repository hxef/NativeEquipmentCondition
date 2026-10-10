#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// Safe readers, writers and calls for a menu movie's objects, and text
// written for it. Each one that takes a clip checks that it is an object
// first, so a clip a UI replacer removed reads as absent. A number comes
// back whichever of the 3 number types the movie keeps it in.
//
// Every native function NEC gives a movie, each listener and handler, is a
// global. Scaleform counts references to it and its count starts at 1,
// which is never given back, so it lives as long as the plugin.
namespace Flash
{
	using Value = Scaleform::GFx::Value;

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

	// A clip's bounds in the stage's units, with x, y, width and height.
	// Nothing for a clip outside the display list, which has no stage.
	[[nodiscard]] std::optional<Value> StageBounds(Value& a_clip);

	// a_text with &, < and > written for an htmlText field.
	[[nodiscard]] std::string HtmlEscaped(std::string_view a_text);
}
