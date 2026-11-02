#include "Gameplay/SpawnCondition/Band.h"

#include "Condition/Condition.h"
#include "Core/Settings.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace SpawnCondition
{
	namespace
	{
		// The limits of the ordinary roll, whatever middle it is centred on.
		// UPSET_CHANCE in Band.h can go past them.
		constexpr float SPAWN_FLOOR = 0.15F;
		constexpr float SPAWN_CEILING = 1.00F;

		// The slider's ends, -100 and 100.
		constexpr std::int32_t MOST_SHIFT = 100;

		// The worst an item arrives at, see WORST_SPAWN.
		float Worst()
		{
			return std::max(Condition::MIN_HEALTH, WORST_SPAWN);
		}

		// Below 0 the floor moves toward the worst an item arrives at by the
		// slider's share, so at -100 the worn end's band fits above it. Above
		// 0 it stays, since a floor rising with the middles would reach the
		// top first and pile every roll there.
		Ends OrdinaryEnds(float a_share)
		{
			const auto plain = std::max(SPAWN_FLOOR, WORST_SPAWN);
			const auto floor = a_share < 0.0F ? std::lerp(plain, Worst(), -a_share) : plain;
			return { floor, std::max(floor, std::min(SPAWN_CEILING, Condition::MAX_HEALTH)) };
		}
	}

	std::int32_t LootShift()
	{
		return std::clamp(Settings::iLootShift.GetValue(), -MOST_SHIFT, MOST_SHIFT);
	}

	Band OrdinaryBand(float a_centre, std::int32_t a_shift)
	{
		const auto share = static_cast<float>(std::clamp(a_shift, -MOST_SHIFT, MOST_SHIFT)) / static_cast<float>(MOST_SHIFT);
		const auto ends = OrdinaryEnds(share);
		const auto [floor, ceiling] = ends;

		// Each item's middle moves that share of the way toward the centre of
		// the band at the end the slider points to, 1 to 25% or 76 to 100%,
		// so every step moves every item and the slider's ends are where the
		// bands end. A middle already past that centre comes back to it, and
		// its band stays at the end.
		auto centre = a_centre;
		if (share < 0.0F) {
			centre = std::lerp(a_centre, Worst() + SPREAD, -share);
		} else if (share > 0.0F) {
			centre = std::lerp(a_centre, ceiling - SPREAD, share);
		}

		// The band shifts to stay inside the limits rather than being clipped,
		// since clipping would put every roll outside onto the limit, and a
		// weapon exactly at the top looks like one this mod never touched. A
		// Courser's band would put half its rolls there.
		auto low = centre - SPREAD;
		auto high = centre + SPREAD;
		if (high > ceiling) {
			low -= high - ceiling;
			high = ceiling;
		}
		if (low < floor) {
			high += floor - low;
			low = floor;
		}
		return { centre, ends, low, std::max(low, high) };
	}

	Rolled RollHealth(const Aim& a_aim)
	{
		// Several threads build inventories at once, so each has its own
		// generator, seeded from the system's random source so reloading a save
		// does not roll the same conditions again.
		static thread_local std::mt19937 engine{ std::random_device{}() };

		// Exactly 1.0 is excluded, since SetHealthPerc treats it as deleting
		// the health and the item would be rolled again the next time it
		// changed hands. Exactly 0.0 is excluded too, see WORST_SPAWN. A
		// trader's own stock comes no worse than the worst the trader stocks,
		// see StockBand.
		auto       lowest = Worst();
		const auto highest = Condition::MAX_HEALTH;
		if (a_aim.stock) {
			lowest = std::clamp(a_aim.stock->worst, lowest, highest);
		}

		std::bernoulli_distribution rare{ UPSET_CHANCE };
		if (rare(engine)) {
			std::uniform_real_distribution<float> whole{ lowest, highest };
			return { std::clamp(whole(engine), lowest, highest), true };
		}

		// A trader's own stock rolls in the trader's band, see
		// VendorRepair/Upkeep.h.
		if (a_aim.stock) {
			const auto low = std::clamp(a_aim.stock->low, lowest, highest);
			const auto high = std::clamp(a_aim.stock->high, low, highest);
			std::uniform_real_distribution<float> band{ low, high };
			return { std::clamp(band(engine), low, high), false };
		}

		const auto shift = LootShift();
		const auto ordinary = OrdinaryBand(a_aim.centre, shift);
		const auto [floor, ceiling] = ordinary.ends;

		std::uniform_real_distribution<float> band{ ordinary.low, ordinary.high };
		return { std::clamp(band(engine), floor, ceiling), false, shift, ordinary.centre };
	}
}
