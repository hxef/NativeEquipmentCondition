#pragma once

#include "Core/Plugin.h"

// The player's companions and settlers, whose weapons wear at
// fCompanionWearRateMult and fSettlerWearRateMult and whose armor wears at
// fCompanionArmorWearRateMult and fSettlerArmorWearRateMult, on top of the NPC
// rates. Read from the actor at each shot, blow or hit, so it keeps no forms
// and has nothing to unload.
namespace Crew
{
	// The kind of gear a share is for, each with its own 2 sliders. Armor
	// covers clothing too.
	enum class Gear
	{
		kWeapons,
		kArmor,
	};

	// What an actor's wear is multiplied by, on top of the NPC rates, and who
	// they are to the player for the trace line, null for anyone else at 1.
	struct Share
	{
		float       mult;
		const char* who;
	};

	// The share of a_actor for a_gear, each slider kept between 0 and 1. A
	// teammate takes the companion speed first, so a companion who lives in a
	// settlement wears as a companion. Then an actor of a settlement the
	// player owns takes the settler speed. The player and anyone else take 1.
	// Reads no inventory, so it runs before any inventory lock.
	[[nodiscard]] Share Of(const RE::Actor& a_actor, Gear a_gear);

	// Whether gear at a_share wears at all, false at 0. Says which speed it
	// takes in the NPC trace log for a companion or a settler.
	[[nodiscard]] bool Wears(const Share& a_share);
}
