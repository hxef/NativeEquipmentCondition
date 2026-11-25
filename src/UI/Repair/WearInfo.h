#pragma once

#include "Core/Plugin.h"

// srm wearinfo: the wear rate the item card shows, with each factor it is
// made of, so a player can see why a weapon or a piece wears as it does. The
// factors multiply to the rate.
//
//   srm wearinfo         the weapon in hand
//   srm wearinfo armor   every worn piece that wears, 1 line each
namespace WearInfo
{
	// Prints the lines to the console and the trace log, or 1 line saying
	// there is nothing that wears.
	void Print(RE::PlayerCharacter& a_player, RE::ConsoleLog* a_console, bool a_armor);
}
