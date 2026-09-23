#pragma once

#include "Core/Plugin.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

// Where a measurement ranks against the rest. Private to this folder.
namespace Provenance
{
	// What a half that could not be measured counts as: the middle, so one half
	// missing pulls the answer to the plain average and not to an end of the
	// scale.
	inline constexpr float UNKNOWN_HALF = 0.5F;

	// The smallest difference worth ranking on. Equal measurements share a
	// place, and equal has to mean equal to a person: an average carries a
	// fraction that depends on how many entries went in, so 2 factions given
	// the same armor through lists of different lengths land 0.0001 apart.
	// Ranked on exact equality, that split 460 characters in one armor set into
	// 2 groups and put super mutants 10% of the scale above raiders wearing the
	// same thing. A point of armor and a cap are the smallest units the game
	// uses.
	inline constexpr float RUNG = 1.0F;

	inline float OnARung(float a_measure)
	{
		return std::round(a_measure / RUNG) * RUNG;
	}

	// Where each measurement ranks against the rest, since a measurement means
	// nothing alone: 300 caps is a fortune in vanilla and pocket change in a
	// load order full of high end weapons. Equal measurements share a position,
	// and a single measurement lands in the middle.
	template <class K>
	std::unordered_map<K, float> PositionsOf(const std::vector<std::pair<K, float>>& a_measured)
	{
		std::unordered_map<K, float> positions;
		positions.reserve(a_measured.size());

		std::vector<float> sorted;
		sorted.reserve(a_measured.size());
		for (const auto& [key, measure] : a_measured) {
			sorted.push_back(measure);
		}
		std::sort(sorted.begin(), sorted.end());

		if (sorted.size() < 2) {
			for (const auto& [key, measure] : a_measured) {
				positions.emplace(key, UNKNOWN_HALF);
			}
			return positions;
		}

		const auto last = static_cast<float>(sorted.size() - 1);
		for (const auto& [key, measure] : a_measured) {
			// The middle of the run of equal measurements, so 100 lists worth
			// the same land together.
			const auto first = std::lower_bound(sorted.begin(), sorted.end(), measure) - sorted.begin();
			const auto after = std::upper_bound(sorted.begin(), sorted.end(), measure) - sorted.begin();
			const auto middle = (static_cast<float>(first) + static_cast<float>(after - 1)) * 0.5F;
			positions.emplace(key, middle / last);
		}
		return positions;
	}
}
