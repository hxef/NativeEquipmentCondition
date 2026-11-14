#pragma once

#include "Core/Plugin.h"

#include "Gameplay/HealthDamage/Curve.h"

#include <atomic>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <string_view>

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
	// of the NPC log it falls in. Each shot opens a block, so comparing inside
	// the block keeps 1 line per shot and still collapses a shotgun's
	// pellets into one. The form ID keeps 2 NPCs with the same gun apart.
	std::uint64_t NpcScope(const RE::TESForm& a_who);

	// The end of a damage line for a copy whose file in NEC/Wear sets its
	// floor, "  keeps 0.50 at 0 by Wear/A_First.ini line 6", and nothing for
	// any other. Worded as the line is written, so a closed log builds no
	// text on a hit.
	struct FloorTail
	{
		Floor floor;
	};
}

// Lets a FloorTail stand in a format string as {:s}.
template <>
struct std::formatter<HealthDamage::FloorTail> : std::formatter<std::string_view>
{
	auto format(const HealthDamage::FloorTail& a_tail, std::format_context& a_context) const
	{
		return std::formatter<std::string_view>::format(WearList::DamageTail(a_tail.floor), a_context);
	}
};
