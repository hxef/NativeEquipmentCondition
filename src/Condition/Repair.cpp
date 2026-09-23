#include "Condition/Repair.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>

namespace Repair
{
	float Debt(std::uint32_t a_percent, float a_multiple)
	{
		const auto reading = static_cast<float>(std::min(a_percent, FULL)) /
		                     static_cast<float>(FULL);
		const auto sound = std::exp(-FALL_OFF);
		return a_multiple * (std::exp(-FALL_OFF * reading) - sound) / (1.0F - sound);
	}

	std::uint32_t Whole(float a_units)
	{
		return a_units > 0.0F ? static_cast<std::uint32_t>(std::lround(a_units)) : 0U;
	}

	std::vector<std::uint32_t> Above(std::uint32_t a_percent)
	{
		std::vector<std::uint32_t> out;
		for (const auto level : LEVELS) {
			if (level > a_percent) {
				out.push_back(level);
			}
		}
		return out;
	}

	const char* NameOf(std::uint32_t a_level)
	{
		for (std::size_t i = 0; i < std::size(LEVELS); i++) {
			if (LEVELS[i] == a_level) {
				return LEVEL_NAME[i];
			}
		}
		return nullptr;
	}

	std::string Ladder(float a_multiple)
	{
		std::string out;
		for (const auto level : LEVELS) {
			out += std::format("{:s}{:d}%:{:.2f}", out.empty() ? "" : " ", level, Debt(level, a_multiple));
		}
		return out;
	}
}
