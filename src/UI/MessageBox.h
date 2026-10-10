#pragma once

#include "Core/Plugin.h"

#include <string>
#include <string_view>
#include <vector>

// A question on the screen with as many answers as it is given. The game's own
// message boxes take 4 buttons, a limit of the one function that builds them,
// and the box draws its buttons in a scrolling list. A repair offers up to 10
// levels and a cancel button, so this uses the function underneath.
//
// A message box mod can draw every box in a style of its own. One gives each
// box it does not know by its words a default style, and its install presets
// make that style's background 0.07, nearly see-through, so NEC's question is
// hard to read over the item card under it. So each frame NEC's own box is on
// screen, a background below 0.075 is drawn at 0.9 with its colour at a tenth,
// as that mod draws a box it makes solid. Every other box, a background that
// is solid enough already and a box with no such style, the game's own among
// them, are left as they are. Roles/Conventions.h names the mod.
namespace MessageBox
{
	// Adds the frame listener that keeps NEC's box solid to the message box
	// movie.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);

	// What a box calls back into. The game owns it once handed over and frees
	// it with its own allocator, so a class built on it uses the heap macro.
	// While one lives, the box it belongs to counts as NEC's own.
	class Callback : public RE::IMessageBoxCallback
	{
	public:
		Callback();
		~Callback() override;

		Callback(const Callback&) = delete;
		Callback& operator=(const Callback&) = delete;
	};

	// Puts the question up and hands a_callback the number of the button
	// pressed, counting from 0. The last button is cancel, and the Cancel key,
	// Tab or B, presses it too. When the box never opens, the callback gets
	// the cancel button and is freed here. The box pauses whatever is under
	// it.
	void Ask(const char* a_title, const char* a_body,
		const std::vector<std::string>& a_buttons, Callback* a_callback);
}
