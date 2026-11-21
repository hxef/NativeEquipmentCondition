#pragma once

#include "Core/Plugin.h"
#include "UI/Repair/VendorRepair/SignPlace.h"
#include "UI/Roles/TraderSide.h"

#include <optional>
#include <string_view>

// The sign where the player moves it, see Sign.h. With fTraderSignX or
// fTraderSignY not 0 it takes the form Place picks, or the first the screen
// can show where none has room, at its own place moved by those HUD units, x
// right and y up, and pulled back until the screen holds all of it. No room
// test then, so it may cover other things, as a moved CND bar may.
//
// A moved sign leaves the trader's side for a clip of NEC's right above it,
// with the side's place, scale and colour, so the side's colour box never
// stretches over the item card or the player's list. That clip takes a colour
// target of its own, see SignBack.h, so without NEC's delete hook the sign
// stays at its own place. Private to this folder.
namespace VendorRepair::Sign
{
	// How far the player moves the sign, in HUD units, 1280 across and 720
	// down on any screen: x right and y up.
	struct Move
	{
		double x = 0.0;
		double y = 0.0;

		bool operator==(const Move&) const = default;
	};

	// The move fTraderSignX and fTraderSignY set, read at every Place.
	// Nothing at 0 and 0, where NEC places the sign itself. Nothing too while
	// NEC does not hold the delete hook, or the side is turned, so its box
	// would not follow the screen's.
	[[nodiscard]] std::optional<Move> MoveOf(const Roles::TraderSide::Room& a_room);

	// Puts the sign a_by from its own place: a_own's form, or with a_own
	// empty the first FirstOf finds. Nothing, with nothing changed, when the
	// movie refuses the moved sign's clip, so the sign stays at its own
	// place. Else whether the line's title row is in play, as Place keeps it.
	[[nodiscard]] std::optional<bool> PlaceMoved(RE::BarterMenu& a_menu, Roles::TraderSide::Side& a_side, Roles::TraderSide::Room& a_room,
		Clips& a_clips, const std::optional<Choice>& a_own, const Move& a_by);

	// Takes the moved sign's clip out with its colour target, a_how saying
	// why. The forms it holds go where Place puts them next.
	void MovedOff(RE::BarterMenu& a_menu, std::string_view a_how);

	// Takes the moved sign's clip out and off a_menu, for a list rebuilt or
	// a screen opening or closing. Its colour target goes with the others.
	void ForgetMoved(RE::BarterMenu& a_menu);
}
