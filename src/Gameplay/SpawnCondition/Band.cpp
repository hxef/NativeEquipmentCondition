#include "Gameplay/SpawnCondition/Band.h"

#include "Condition/Condition.h"

#include <algorithm>
#include <random>

namespace SpawnCondition
{
	namespace
	{
		// The limits of the ordinary roll, whatever middle it is centred on.
		// UPSET_CHANCE in Band.h can go past them.
		constexpr float SPAWN_FLOOR = 0.15F;
		constexpr float SPAWN_CEILING = 1.00F;
	}

	Ends OrdinaryEnds()
	{
		const auto floor = std::max(SPAWN_FLOOR, WORST_SPAWN);
		return { floor, std::max(floor, std::min(SPAWN_CEILING, Condition::MAX_HEALTH)) };
	}

	Rolled RollHealth(float a_centre)
	{
		// Several threads build inventories at once, so each has its own
		// generator, seeded from the system's random source so reloading a save
		// does not roll the same conditions again.
		static thread_local std::mt19937 engine{ std::random_device{}() };

		// Exactly 1.0 is excluded, since SetHealthPerc treats it as deleting
		// the health and the item would be rolled again the next time it
		// changed hands. Exactly 0.0 is excluded too, see WORST_SPAWN.
		const auto lowest = std::max(Condition::MIN_HEALTH, WORST_SPAWN);
		const auto highest = Condition::MAX_HEALTH;

		std::bernoulli_distribution rare{ UPSET_CHANCE };
		if (rare(engine)) {
			std::uniform_real_distribution<float> whole{ lowest, highest };
			return { std::clamp(whole(engine), lowest, highest), true };
		}

		const auto [floor, ceiling] = OrdinaryEnds();

		// The band shifts to stay inside the limits rather than being clipped,
		// since clipping would put every roll outside onto the limit, and a
		// weapon exactly at the top looks like one this mod never touched. A
		// Courser's band would put half its rolls there.
		auto low = a_centre - SPREAD;
		auto high = a_centre + SPREAD;
		if (high > ceiling) {
			low -= high - ceiling;
			high = ceiling;
		}
		if (low < floor) {
			high += floor - low;
			low = floor;
		}

		std::uniform_real_distribution<float> band{ low, std::max(low, high) };
		return { std::clamp(band(engine), floor, ceiling), false };
	}
}
