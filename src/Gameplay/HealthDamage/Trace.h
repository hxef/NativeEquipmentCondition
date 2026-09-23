#pragma once

#include "Core/Plugin.h"

#include <atomic>
#include <cstdint>
#include <initializer_list>

// Saying so in the trace logs. Private to this folder.
namespace HealthDamage
{
	// Whether a line says exactly what the last one from the same place said.
	// Combat keeps asking whether one actor's blow would kill another, several
	// times a second, with a test blow built from the equipped weapon, and for
	// the player every one is the same line. The numbers are hashed into one
	// value and swapped in one atomic step, so 2 threads cannot mix up their
	// values. a_scope keeps lines from different scopes apart. The player's
	// lines leave it 0, and NpcScope is what the NPC log passes.
	bool Repeats(std::atomic<std::uint64_t>& a_last, std::initializer_list<float> a_values, std::uint64_t a_scope = 0);

	// The scope an NPC's line is compared in: whose line it is and which block
	// of the NPC log it falls in. An NPC's weapon never wears, so every shot
	// reads the same, and each shot opens a block, so comparing inside the
	// block keeps one line per shot and still collapses a shotgun's pellets
	// into one. The form ID keeps 2 NPCs with the same gun apart.
	std::uint64_t NpcScope(const RE::TESForm& a_who);
}
