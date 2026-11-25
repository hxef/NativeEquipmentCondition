#pragma once

#include "Core/Plugin.h"

#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Hud/PowerArmorCondition/Layout.h"

#include <cstdint>
#include <optional>
#include <string>

// The dash at Power armor dash CND Percent: CND over the percent, right of the
// ammo box, each line level with its row of digits, turned with the dash. The
// bar's boxes and its word hide, and the readout's 2 fields of Percent hold the
// lines, see HudParts::Readout::Extra. The place sliders move the 2 lines as
// they move the bar. With bPowerArmorBarShowsWord off CND hides and the
// percent stands alone halfway between the 2 rows, level with the bar.
// Private to this folder.
namespace PowerArmorCondition
{
	// The 2 lines' size, in rows of the dash. In the HUD's bold font it gives
	// capitals about 16 of the 37.5 pixels between the 2 rows of digits on a
	// 1080p shot, 0.19 of the box's height. That is taller than the dash's AMMO
	// title, whose capitals are 11 to 12 pixels without their glow. No movie
	// holds AMMO, since the box is part of the model, so both are read off
	// shots.
	inline constexpr double LINES_SIZE = 0.6;

	// How wide their letters are drawn, as a share of the font's own, AMMO's
	// width.
	inline constexpr double LINES_WIDE = 0.86;

	// The 2 lines of 1 HUD movie, kept between frames so only a change is
	// written.
	class Lines
	{
	public:
		// Writes CND at the size a_box's rows ask for. Called again only when
		// the dash is drawn at a new size, since it measures text. False when
		// the movie refuses a field.
		bool Lay(Scaleform::GFx::Movie& a_movie, HudParts::Value& a_readout, const Box& a_box);

		// Writes the percent for a_tenths when it changed, and returns the
		// whole text drawn then, "CND 87.4%", or "87.4%" without a_worded,
		// for the trace.
		std::optional<std::string> Write(Scaleform::GFx::Movie& a_movie, HudParts::Value& a_readout, std::uint32_t a_tenths, bool a_worded);

		// Puts each line's letters' left end and middle on its point, a_upper
		// and a_lower in the movie's units, through the readout's own place,
		// a_origin turned a_turn radians, so they turn with it. Without
		// a_worded CND hides and the percent goes halfway between the 2
		// points. Returns whether the lines shown stand whole inside a_frame.
		bool Place(HudParts::Value& a_readout, const Frame& a_frame, const Point& a_origin, double a_turn, const Point& a_upper, const Point& a_lower, bool a_worded);

		// Shows or hides the 2 lines once made, and the bar the other way
		// round. Shown also hides the bar's word, which its own switch shows
		// again at Bar.
		void Show(HudParts::Value& a_readout, bool a_shown);

		// The size the lines are written at, 0 before Lay.
		[[nodiscard]] double Size() const { return size; }

		// Forgets everything, for a new HUD movie or a change of form.
		void Reset();

	private:
		double      size{ 0.0 };
		std::string word;
		std::string number;

		// Whether CND shows, as Place last set it, nothing since Show.
		std::optional<bool> worded;

		// Where each field's letters sit from its own top left corner, as
		// last written.
		HudParts::Readout::LabelPlace wordAt;
		HudParts::Readout::LabelPlace numberAt;
	};
}
