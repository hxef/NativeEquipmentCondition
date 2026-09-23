#pragma once

#include "Core/Plugin.h"

// A CND bar on the power armor dash. Inside power armor the game hides its ammo
// counter and shows a dash, so the counter's CND bar goes with it. This puts a
// bar back where the counter has it, between the rounds in the gun and the
// rounds in reserve. The bar runs between the 2 rows of digits in the dash's
// ammo box, as wide as the digits and as deep as the room between them allows,
// and the word stands inside the box past the bar's right end, level with the
// bar, sized to fit before the box's right edge, see Layout.h. No panel is
// drawn around it, since the dash's own panels are part of the model, lit and
// worn, and a flat copy never matches.
//
// The dash is not a menu but a model, Interface/Objects/PADashDials01.nif,
// carried in front of the camera, so the bar is drawn in the HUD movie, which
// keeps drawing over the dash, as the low battery warning does. The dash sits
// at a distance that depends on the screen shape, 350 units on most, 385 on 16
// by 10, 300 wider than 2 to 1, and 3 ini keys move it, so 3 of its own digits
// are asked where they are: AmmoCountHunds:0, the first digit of the reserve,
// AmmoCountOnes:0, the last, which gives a digit's width, and ClipCountHunds:0
// on the row above, which gives a row's depth. The dash lives in a scene of its
// own with a camera of its own, held by PowerArmorRenderer, so that camera is
// asked. No renderer means no dash.
//
// The bar is laid out in digits across and rows down, the dash's own measure,
// and put on the screen through the same camera, so it lands in the right place
// at the dash's own slant on any screen, field of view or ini key. The dash
// sways as the player breathes, so it is measured every frame and the bar
// follows, its word anti-aliased for animation. Only its size waits for a
// change before it is laid out again, since that means measuring text. If
// anything fails, or the bar would fall outside the movie, nothing is drawn.
//
// Dash.cpp finds the dash through its camera, and Layout.cpp lays the bar out
// in the movie. Dash.h and Layout.h are what they share.
namespace PowerArmorCondition
{
	// Writes a trace line each time the player steps out of power armor, from
	// the thread that lets go of the dash. Only while there is a trace.
	void Load();

	// Adds the bar to the HUD movie and ignores every other movie.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);
}
