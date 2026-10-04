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

	// The condition of the weapon whose item card is being drawn, recorded at
	// the display site in Combat.cpp and read by the card's blast hook in
	// Card.cpp, on the thread drawing the card. Reading it clears it to -1.
	float DisplayHealth();
}
