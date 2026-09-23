#include "Gameplay/HealthDamage/Trace.h"

#include "Core/TraceLog.h"

#include <bit>

namespace HealthDamage
{
	bool Repeats(std::atomic<std::uint64_t>& a_last, std::initializer_list<float> a_values, std::uint64_t a_scope)
	{
		// FNV-1a, which is an exclusive or and a multiply per number.
		std::uint64_t hash = (0xCBF29CE484222325ULL ^ a_scope) * 0x100000001B3ULL;
		for (const auto value : a_values) {
			hash = (hash ^ std::bit_cast<std::uint32_t>(value)) * 0x100000001B3ULL;
		}

		return a_last.exchange(hash, std::memory_order_relaxed) == hash;
	}

	std::uint64_t NpcScope(const RE::TESForm& a_who)
	{
		return (std::uint64_t{ TraceLog::Npc::Blocks() } << 32) | a_who.formID;
	}
}
