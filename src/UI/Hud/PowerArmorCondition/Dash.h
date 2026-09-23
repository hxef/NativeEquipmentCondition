#pragma once

#include "Core/Plugin.h"

// Finding the dash through its own camera. Private to this folder.
namespace PowerArmorCondition
{
	// Why the dash could not be measured, so the log says which step failed.
	enum class Miss
	{
		kNone,
		kNoRenderer,
		kNoDash,
		kNoDigits,
		kBehindCamera,
		kNotFacing,
	};

	// A place on the screen as fractions of it, 0 at the left and top, 1 at the
	// right and bottom. Places in the HUD movie are kept the same way in the
	// movie's units.
	struct Point
	{
		double x = 0.0;
		double y = 0.0;
		bool   ok = false;  // whether it could be placed at all
	};

	// Where the dash is, as places on the screen. Measured through F4SE's task
	// queue on a worker thread and read by the HUD's frames, hence the lock.
	struct Anchor
	{
		Point start{};                   // the bar's left end, halfway down it
		Point end{};                     // its right end
		Point word{};                    // where the word starts, level with the bar
		Point limit{};                   // the furthest the word may reach
		Point first{};                   // the reserve row's first digit
		Point above{};                   // the first digit of the row above
		Miss  miss = Miss::kNoRenderer;  // why none of it is there, when it is not
	};

	// Whether the player was in power armor when the dash was last
	// measured. See g_inPowerArmor in Dash.cpp.
	[[nodiscard]] bool InPowerArmor();

	// Whether the current HUD mode lets the dash be seen.
	[[nodiscard]] bool DashAllowed();

	// Asks F4SE's task queue to measure where the dash is. Called every frame,
	// and asks for nothing while the last measurement has not arrived yet.
	void QueueAnchor();

	// Where the dash was, as the last measurement left it.
	[[nodiscard]] Anchor CurrentAnchor();
}
