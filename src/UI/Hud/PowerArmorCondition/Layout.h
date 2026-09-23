#pragma once

#include "Core/Plugin.h"

#include "UI/Hud/PowerArmorCondition/Dash.h"

#include <numbers>

// Laying the bar out in the HUD movie. Private to this folder.
namespace PowerArmorCondition
{
	// The largest size of the word, in rows, so it grows with the dash. At this
	// size its capitals stand as tall as the dash's own AMMO. The word is then
	// wider than the room in the box, so it is drawn smaller to fit, see
	// Layout.
	inline constexpr double LABEL_SIZE = 0.4;

	// How wide the word's letters are drawn, as a share of the font's own.
	// The dash's AMMO is narrower than the font, and a tenth off brings C, N
	// and D to the width of its letters.
	inline constexpr double LABEL_WIDE = 0.9;

	// How deep the bar is, in rows. The lit digits of the 2 rows leave 0.3 of
	// a row between them at the least, a 2, 3 or 4 over a 4. The bar and the
	// dark shadow the HUD draws under it, 0.05 of a row, keep about 0.04 clear
	// of each. It covers the ends of the tubes' glass.
	inline constexpr double BAR_DEEP = 0.17;

	// Degrees in a radian. The turn is worked out in radians and Flash turns a
	// clip by degrees.
	inline constexpr double DEGREES = 180.0 / std::numbers::pi;

	// What part of the HUD movie is on screen, and how to turn a place on the
	// screen into a place in the movie, done the way the game places floating
	// quest markers. Up to 2 to 1 the movie is stretched across the whole
	// screen. Wider, it keeps the shape of a 16 by 9 screen in the middle, so
	// the fraction is worked out again from the centre.
	struct Frame
	{
		double left = 0.0;
		double top = 0.0;
		double width = 0.0;
		double height = 0.0;
		double aspect = 16.0 / 9.0;

		[[nodiscard]] double StageX(double a_screenX) const
		{
			const auto across = aspect < 2.0 ? a_screenX : (((aspect * 0.5625) * ((a_screenX * 2.0) - 1.0)) + 1.0) * 0.5;
			return left + (width * across);
		}

		[[nodiscard]] double StageY(double a_screenY) const
		{
			return top + (height * a_screenY);
		}

		[[nodiscard]] Point Stage(const Point& a_screen) const
		{
			return Point{ StageX(a_screen.x), StageY(a_screen.y), a_screen.ok };
		}

		[[nodiscard]] bool Holds(const Point& a_place) const
		{
			return a_place.x >= left && a_place.x <= left + width && a_place.y >= top && a_place.y <= top + height;
		}
	};

	[[nodiscard]] Frame FrameOf(Scaleform::GFx::Movie& a_movie);

	// Where the bar lands, in the movie's coordinates. It runs between the ammo
	// box's 2 rows of digits, a little off level, so it is kept as where it
	// starts and how far it is turned.
	struct Box
	{
		double x = 0.0;      // the bar's left end, halfway down it
		double y = 0.0;
		double width = 0.0;  // how far the bar runs
		double gap = 0.0;    // how far past the bar's right end the word starts
		double room = 0.0;   // how far past the bar's right end the word may reach
		double row = 0.0;    // one row of the dash, which the rest is sized by
		double turn = 0.0;   // how far it is turned from level, clockwise, in radians

		// A place so far along the bar from its left end and so far below its
		// middle.
		[[nodiscard]] Point At(double a_along, double a_below) const
		{
			const auto cos = std::cos(turn);
			const auto sin = std::sin(turn);
			return Point{ x + (a_along * cos) - (a_below * sin), y + (a_along * sin) + (a_below * cos), true };
		}
	};

	// How the bar and its word came out, worked out at layout and wanted every
	// frame. The readout's origin is the right end of the bar, so it sits the
	// bar's width along from the bar's left end.
	struct Bar
	{
		double width = 0.0;  // how far back to the left the bar runs
		double deep = 0.0;   // how deep it is
		double size = 0.0;   // how big the word is
		double reach = 0.0;  // how far past the bar's right end the word ends
	};

	[[nodiscard]] Box Place(const Frame& a_frame, const Anchor& a_anchor);

	// Whether a box is the same size as the one laid out. Where it sits is left
	// out: the dash sways every frame and the bar is moved, not laid out again.
	[[nodiscard]] bool SameSize(const Box& a_box, const Box& a_other);

	// Lays the bar out for a size of dash and returns how it came out. Called
	// again only when the dash is drawn at a different size, since it measures
	// text.
	Bar Layout(Scaleform::GFx::Value& a_readout, const Box& a_box);
}
