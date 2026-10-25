#pragma once

#include "Core/Plugin.h"

// What the files of this feature share. Private to this folder.
namespace HealthDamage
{
	// Patches the blow, in Combat.cpp: its physical damage, what combat reads
	// as the weapon in hand, and its damage types.
	void InstallCombat();

	// Patches the cast that applies a blow's or a shot's object effects to the
	// target, in Effect.cpp.
	void InstallEffect();

	// Patches an explosion's damage and the cast of its object effect, in
	// Blast.cpp.
	void InstallBlast();

	// Patches the damage the item card prints, in Card.cpp.
	void InstallCard();

	// In Card.cpp: the health the display site in Combat.cpp scales a card's
	// physical damage by, a_condition, or -1 for a weapon on a list in
	// NEC/Exclude. The card's blast hook reads the same health once, on the
	// thread drawing the card.
	float DisplayHealth(float a_condition);
}
