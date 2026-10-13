#pragma once

#include "Condition/Condition.h"
#include "Core/Plugin.h"

// The health of the weapon in the player's hands, for hooks that run every
// frame from wherever the game updates: the animation's fire speed, the
// crosshair and the VATS hit chance. The first ask after the last task
// measures it on the spot, so it is right even when nothing asked for a
// while, as VATS opening needs. Later asks reuse that reading until the
// queued task runs, up to a frame later. While another thread holds the
// player's inventory, the last measurement stands until the task measures.
namespace InHand
{
	// Forgets the last measurement. Form IDs name something else after a full
	// reset and once another game starts, so it runs then too, see ForgetPlay
	// in main.cpp.
	void Unload();

	// The measurement: the weapon's form ID, 0 for none that wears, and its
	// health.
	struct Reading
	{
		RE::TESFormID weapon{ 0 };
		float         health{ Condition::INVALID_HEALTH };
	};

	[[nodiscard]] Reading Now();

	// The health measured for a_weapon, or Condition::INVALID_HEALTH, which
	// counts as new, when the measurement is for another weapon.
	[[nodiscard]] float Health(const RE::TESForm& a_weapon);
}
