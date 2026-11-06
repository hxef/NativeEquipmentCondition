#pragma once

#include "Core/Plugin.h"

// Worn guns jam, as they did in Fallout 3 and New Vegas.
//
// Each shot the player's gun is about to fire rolls first. A jammed shot is
// never fired: no round, no bang, no wear. The magazine empties instead, as if
// its last round had gone, and the game handles it as ever: an automatic weapon
// lets go of the trigger, the next pull reloads, the gun clicks, and the HUD
// says it jammed. Emptying costs no ammo, since the rounds stay counted in the
// inventory until fired.
//
// The chance is per magazine. It is 0 at fJamStart and above, and below it
// rises in a straight line to fJamChance at 0 condition, as the other weapon
// penalties run from their start to their floor. fWeaponPenaltyStart does not
// move it. At the defaults, 1 and 0.5, a gun at 45% jams in 1 magazine of 10,
// at 25% in every 2nd and at 5% in 9 of 10, whatever the magazine's size.
// Which round jams is chance too: in a magazine of 500 that is sure to jam,
// the first round jams with 1 in 500, the next with 1 in 499, and the last for
// certain. ChancePerShot in Jam.cpp does the same for chances below 1. A
// chance or a start of 0 turns jams off, and nothing rolls.
//
// Guns that fire once per reload, the Fat Man, the Flare Gun, the Syringer and
// the Laser Musket, jam as the reload finishes instead, so a jam costs one
// reload and never the shot. The reload after a jammed one always loads.
//
// Both settings are read on every roll, so the MCM page retunes them on the
// spot.
namespace Jam
{
	// Patches the reload call. Without it only the guns that fire once per
	// reload never jam.
	void Install();

	// Reads the message text.
	void Load();

	// Forgets the gun whose last reload jammed, also when another game
	// starts, see ForgetPlay in main.cpp.
	void Unload();

	// Rolls for a jam on one shot of the player's gun, before the engine fires
	// it. Guns that fire once per reload never jam here, they jam as the
	// reload finishes. a_health is the gun's, as the fire call read it. True
	// when it jammed: the magazine is empty, the player has been told, and the
	// caller must not fire the shot.
	bool Roll(RE::PlayerCharacter& a_player, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, std::uint32_t a_equipIndex,
		float a_health);

	// Whether guns can jam at all, false while fJamChance or fJamStart is 0.
	// The loading tip about jams asks it too.
	[[nodiscard]] bool Jams();
}
