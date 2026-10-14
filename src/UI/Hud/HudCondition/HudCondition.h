#pragma once

#include "Core/Plugin.h"
#include "UI/Flash.h"

#include <optional>

// A CND bar on the HUD, by the ammo counter. The counter,
// RightMeters_mc.AmmoCount_mc in HUDMenu.swf, shows the rounds in the gun, the
// rounds in reserve and a divider line between them. The bar replaces the
// divider, 3 times as thick so its level is easy to read, shrinks from the
// left as the weapon wears, and the word CND follows past its end. The drawing
// is in HudParts.h, since the power armor readout is the same one at another
// size.
//
// The bar is a sprite beside the counter, not inside it. With a gun out it
// replaces the divider and fades with the counter. With a melee weapon out the
// game hides the counter, so the bar shows alone while the weapon is out. It
// hides when the HUD mode hides the counter, in the Pip-Boy for example, or in
// power armor, where PowerArmorCondition.h takes over. With no condition to
// show, the divider comes back and the HUD is exactly as it was.
//
// A HUD mod can turn the divider, shorten it, or put a number or another part
// where the bar would go. Then the bar sits in the nearest free spot by the
// counter, and the divider shows. A word that would run off screen or cover a
// part moves beside the bar, see Spot.h. It is all measured from the movie a
// few times a second, with no check for any mod, so the bar follows a layout
// that changes. A counter turned as a whole turns the bar with it. The bar
// keeps its last spot while the divider is hidden or off screen.
//
// fHudBarX and fHudBarY move the bar and its word from wherever they sit,
// along the screen in HUD units, and off its spot the divider shows again.
// Whatever they say, the bar and its word stay whole on screen, checked a few
// times a second and on every change, and the bar hides while its counter is
// off screen. The MCM page shows that place from the screen's top left
// corner, also in HUD units, see Mcm/HudBar.cpp.
//
// The bar is a HUD part of its own, with the native object the game gives each
// part, so the HUD tints it with its colour. It then takes the colour the
// divider is drawn in, so a HUD mod that colours the counter or the divider
// colours the bar too. It is updated from the HUD's own frames, which check
// the condition again every few frames, see HudParts::Weapon.
namespace HudCondition
{
	// Adds the bar to the HUD movie through HudParts::Waiter. Ignores every
	// other movie.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);

	// How far the player moved the bar from its spot, from fHudBarX and
	// fHudBarY, in HUD units along the screen, above 0 to the right and up.
	// 0 and 0 is the spot itself.
	struct Shift
	{
		double x = 0.0;
		double y = 0.0;

		bool operator==(const Shift&) const = default;
	};

	// The shift now, each value that is no number counting as 0.
	[[nodiscard]] Shift ShiftNow();

	// a_want held so a box from a_from to a_to stays between a_low and
	// a_high. A box wider than that keeps its start on screen. The HUD holds
	// the bar by it, and the MCM page shows the bar's place by it.
	[[nodiscard]] double Held(double a_want, double a_from, double a_to, double a_low, double a_high);

	// a_value, or 0 when it is no number.
	[[nodiscard]] double Finite(double a_value);

	// The bar's place for the MCM page, in HUD units: the box the bar and its
	// word fill on their spot before the shift, and the visible frame, the
	// part of the HUD's stage the screen shows.
	struct Measure
	{
		Flash::Box box;
		Flash::Box frame;
	};

	// From the HUD's last place with its counter on screen. Nothing before
	// the first since HUDMenu.swf loaded. Safe from any thread.
	[[nodiscard]] std::optional<Measure> Measured();
}
