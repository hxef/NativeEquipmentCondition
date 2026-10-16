#pragma once

#include "Core/Plugin.h"

#include <cstdint>

// Wearing down the copy in hand, which the player's wear and an NPC's share.
// Private to this folder.
namespace WeaponWear
{
	// What WearCopy did.
	struct Worn
	{
		std::uint32_t count{ 0 };  // what the equipped stack held, 0 when none was equipped
		bool          moved{ false };
		bool          broke{ false };  // the health it moved to is broken
	};

	// Wears down a_owner's equipped copy of a_weapon, see Wear in WeaponWear.h.
	// a_theirs for an NPC's copy, see WearTheirs there, and a_keeper the
	// essential NPC holding it, or null. The engine's walk inside takes the
	// inventory lock as a reader, so a caller may hold it as a writer, as
	// TryWear in Theirs.cpp does. Call PutAway only once the lock is let go.
	Worn WearCopy(RE::TESObjectREFR& a_owner, RE::TESObjectWEAP& a_weapon, const char* a_source, float a_scale, bool a_theirs,
		const RE::Actor* a_keeper);
}
