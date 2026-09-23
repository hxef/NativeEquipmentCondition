#pragma once

#include "Core/Plugin.h"

// Everything that wears the player's weapon down as it is used. A gun wears
// once per shot, as the engine's own fire call returns, by the power its rounds
// left with. So a shotgun's pellets cost 1 shot between them, a tap of a Gauss
// rifle costs the share of a full charge it was held to, and a Laser Musket
// costs 1 shot for every crank. A blow struck by hand wears the weapon when it
// lands, since the hit is the only thing that names the weapon and says how
// hard the game made it land. A power attack wears harder.
//
// Nobody else's weapon wears. An NPC's shots and blows are logged to the NPC
// trace log, see TraceLog.h, and read for the pace an automatic weapon keeps,
// see FireRate.h. A reload is watched too: guns that fire once per reload jam
// as the reload finishes, see Jam.h.
//
// Hits.cpp is the hit sink and what a blow was, and Hits.h what it shares with
// WeaponEvents.cpp.
namespace WeaponEvents
{
	// Patches the fire and reload calls.
	void Install();

	// Registers the hit sink and the shot sink once. The game keeps both
	// through every reload and full reset, and a second of either would count
	// everything twice.
	void Load();

	// Whoever the engine's Fire on this thread is firing for, or null outside
	// it. Code that Fire reaches, such as the automatic weapon sound in
	// FireRate.cpp, asks it whose shot it is.
	[[nodiscard]] const RE::TESObjectREFR* Shooter();
}
