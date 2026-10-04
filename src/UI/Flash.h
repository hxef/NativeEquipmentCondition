#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <string>
#include <string_view>

// Reading values from a menu movie's objects, and writing text for it. Every
// readout and menu hook reads clips through these. A number comes back
// whichever of the 3 number types the movie keeps it in. Each accepts whatever
// it is handed, including a clip a UI replacer removed, and returns as if the
// member were not there.
namespace Flash
{
	using Value = Scaleform::GFx::Value;

	// A value as a number, whichever of the 3 number types it is. 0 for
	// anything else.
	[[nodiscard]] double AsNumber(const Value& a_value);

	// A member of a clip as a number, or a_absent when there is none.
	[[nodiscard]] double Number(const Value& a_object, std::string_view a_name, double a_absent = 0.0);

	// A member of a clip as true or false, or false when there is none.
	[[nodiscard]] bool Bool(const Value& a_object, std::string_view a_name);

	// A member of a clip as text, or "(none)" when there is none.
	[[nodiscard]] std::string String(const Value& a_object, std::string_view a_name);

	// How visible a clip is, 0 hidden to 1 shown. The HUD hides parts by fading
	// alpha or switching visible off, so both count.
	[[nodiscard]] double Opacity(const Value& a_clip);

	// A clip's child by name, or a value that is not a display object when
	// there is none.
	[[nodiscard]] Value Child(Value& a_parent, const char* a_name);

	// A clip's child by index from 0, or a value that is not a display object
	// when there is none.
	[[nodiscard]] Value ChildAt(Value& a_parent, std::uint32_t a_index);

	// a_text with &, < and > written for an htmlText field.
	[[nodiscard]] std::string HtmlEscaped(std::string_view a_text);
}
