#pragma once

#include "Core/Plugin.h"

// Worn guns jam. Fallout 3 and New Vegas jammed guns in poor condition. Fallout
// 4 cut the mechanic but still registers the old settings, fWeaponConditionJam1
// to 10 and fWeaponConditionReloadJam1 to 10, with their old values, and the
// weapon record keeps a No Jam After Reload flag. The jam comes back here on
// those settings.
//
// Each shot the player's gun is about to fire rolls first. A jammed shot is
// never fired: no round, no bang, no wear. The magazine empties instead, as if
// its last round had gone, and the game handles it as ever: an automatic weapon
// lets go of the trigger, the next pull reloads, the gun clicks, and the HUD
// says it jammed. Emptying costs no ammo, since the rounds stay counted in the
// inventory until fired.
//
// fWeaponConditionReloadJam is a table of 10 chances, one per 10% of
// condition, worst first: 1, 0.75, 0.5, 0.25 and 0.1 for the 5 steps below
// 50% and 0 from 50% up. Here it is the chance that a magazine jams, so a gun
// below 10% jams once per magazine whatever its size. Which round jams is
// chance too: in a magazine of 500 that is sure to jam, the first round jams
// with 1 in 500, the next with 1 in 499, and the last for certain.
// ChancePerShot in Jam.cpp does the same for chances below 1.
//
// Guns that fire once per reload, the Fat Man, the Flare Gun, the Syringer and
// the Laser Musket, jam as the reload finishes instead, so a jam costs one
// reload and never the shot. The reload after a jammed one always loads.
//
// The settings are read on every roll, so setgs or a plugin retunes all of it.
namespace Jam
{
	// Patches the reload call, a detail of the switch: without it only the
	// guns that fire once per reload stop jamming.
	void Install();

	// Finds the settings and reads the message text.
	void Load();

	// Forgets the gun whose last reload jammed.
	void Unload();

	// Rolls for a jam on one shot of the player's gun, before the engine fires
	// it. Guns that fire once per reload never jam here, they jam as the
	// reload finishes. True when it jammed: the magazine is empty, the player
	// has been told, and the caller must not fire the shot.
	bool Roll(RE::PlayerCharacter& a_player, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, std::uint32_t a_equipIndex);
}
