#pragma once

#include "Core/Plugin.h"

#include <string>
#include <vector>

// A question on the screen with as many answers as it is given. The game's own
// message boxes take 4 buttons, a limit of the one function that builds them,
// and the box draws its buttons in a scrolling list. A repair offers up to 10
// levels and a cancel button, so this uses the function underneath.
namespace MessageBox
{
	// Puts the question up and hands a_callback the number of the button
	// pressed, counting from 0. The last button is cancel, and the Cancel key,
	// Tab or B, presses it too. The game frees the callback with its own
	// allocator, so it has to be built with the heap macro. When the box never
	// opens, the callback gets the cancel button and is freed here. The box
	// pauses whatever is under it.
	void Ask(const char* a_title, const char* a_body,
		const std::vector<std::string>& a_buttons, RE::IMessageBoxCallback* a_callback);
}
