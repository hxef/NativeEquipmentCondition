#pragma once

#include "Core/Plugin.h"

namespace HealthDamage
{
	// Scales weapon damage by the item's health extra data. The engine looks up
	// the equipped item's health on every attack and hands it to
	// CombatFormulas::CalcWeaponDamage, and the Pip-Boy reaches the same
	// function through GetWeaponDisplayDamage. It never uses the number:
	// Skyrim's tempering curve only adds damage and is not used for guns.
	// Install hooks every call to CalcWeaponDamage and scales the result, which
	// also covers the Pip-Boy.
	//
	// A weapon's object effects are spells, the radiation a Radium Rifle
	// applies to what it hits, and the engine casts them at full power. Install
	// multiplies their power by the share of damage the condition leaves, see
	// Curve.h, so they weaken with wear like the rest.
	//
	// Everybody fights at the condition their weapon is in. Only the player's
	// weapons wear, see WeaponEvents.h, so a raider's rusted pipe gun does
	// less damage than a new one.
	//
	// Combat.cpp is the blow, Effect.cpp a weapon's object effects, Blast.cpp
	// an explosion and Card.cpp the item card. Curve.h is the damage line,
	// Trace.h the trace lines, and Hooks.h what they share.
	void Install();
}
