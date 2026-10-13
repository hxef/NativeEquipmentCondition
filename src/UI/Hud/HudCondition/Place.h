#pragma once

#include "UI/Roles/Hud.h"

// Where the readout sits in the counter's parent: on its spot, with the
// counter's place, scale and turn, and moved by the player's shift. Private
// to this folder.
namespace HudCondition
{
	// Where a clip sits, how big it is drawn and how far it is turned.
	struct Transform
	{
		double x = 0.0;
		double y = 0.0;
		double scaleX = 1.0;
		double scaleY = 1.0;
		double rotation = 0.0;

		bool operator==(const Transform&) const = default;
	};

	// How far the player moved the bar from its spot, from fHudBarX and
	// fHudBarY, above 0 to the right and up. 0 and 0 is the spot itself.
	struct Shift
	{
		double x = 0.0;
		double y = 0.0;

		bool operator==(const Shift&) const = default;
	};

	[[nodiscard]] Shift ShiftNow();

	[[nodiscard]] Transform TransformOf(const Roles::Value& a_clip);

	// Puts the readout at its spot, moved by the player's shift in the
	// parent's units. It sits beside the counter in the same parent, so it
	// copies the counter's place, scale and turn, and the spot turns with
	// it. The HUD's y runs down, so the shift's y is taken off.
	void Place(Roles::Value& a_readout, const Transform& a_counter, const Roles::Hud::Line& a_spot, const Shift& a_shift);
}
