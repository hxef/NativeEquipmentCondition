#pragma once

#include "Core/Plugin.h"

#include <string_view>

// Finds the parts of whatever movie draws a menu, by the job each part does:
// the clip the engine holds, then vanilla's place, and for button bars a walk.
// Each has to pass the part's test, and a miss comes back empty. Nothing here
// keeps a Value past the call, since a menu can close before the next frame.
namespace Roles
{
	using Value = Scaleform::GFx::Value;

	// A movie loaded. Forgets the found and noted lines of the last movie with
	// this file name. MenuMovies calls it before the features.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);

	// Whether a clip is on screen: its own visible and alpha above 0, never a
	// parent's, and its bounds on the stage meeting the frame the screen
	// shows, which also catches a parent moved off. Asked only of parts NEC
	// follows into place.
	[[nodiscard]] bool OnScreen(Scaleform::GFx::Movie& a_movie, Value& a_clip);

	// The bounds half of OnScreen, for a part whose visible NEC wrote itself.
	[[nodiscard]] bool InFrame(Scaleform::GFx::Movie& a_movie, Value& a_clip);

	// One trace line per movie load and part, saying how it was found: through
	// the engine, at vanilla's place, by a walk or by a Conventions method.
	void Found(Scaleform::GFx::Movie& a_movie, std::string_view a_part, std::string_view a_how);

	// One trace line per movie load and part for a miss that may not last,
	// saying what the player sees instead.
	void Noted(Scaleform::GFx::Movie& a_movie, std::string_view a_part, std::string_view a_effect);

	// One NEC.log warning per movie file and part for a miss that lasts: a bar
	// after its retry, a container or barter screen with no card, a card
	// redrawn with no CND row, a card with 2, and a HUD with no ammo counter.
	// Says what is missing and what the player sees. The same miss again goes
	// to the trace as Noted does.
	void Missing(Scaleform::GFx::Movie& a_movie, std::string_view a_part, std::string_view a_effect);
}
