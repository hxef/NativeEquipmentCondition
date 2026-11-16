#include "Condition/Repair.h"

#include "Condition/Condition.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>

namespace Repair
{
	std::uint32_t CardTenthsFrom(float a_percent)
	{
		if (!(a_percent > 0.0F)) {
			return 0;
		}
		if (Condition::Decimals() == Condition::CardDecimals::kOff) {
			const auto half = std::round(std::min(a_percent, 100.0F) * 2.0F) / 2.0F;
			return static_cast<std::uint32_t>(std::ceil(half)) * Condition::TENTHS_PER_PERCENT;
		}
		const auto half = std::round(std::min(a_percent, 100.0F) * 2.0F);
		return static_cast<std::uint32_t>(half) * 5;
	}

	std::uint32_t PercentFrom(std::uint32_t a_tenths)
	{
		using Condition::TENTHS_PER_PERCENT;
		return a_tenths / TENTHS_PER_PERCENT + (a_tenths % TENTHS_PER_PERCENT != 0 ? 1 : 0);
	}

	float Debt(std::uint32_t a_percent, float a_multiple)
	{
		return DebtTenths(std::min(a_percent, FULL) * Condition::TENTHS_PER_PERCENT, a_multiple);
	}

	float DebtTenths(std::uint32_t a_tenths, float a_multiple)
	{
		const auto reading = static_cast<float>(std::min(a_tenths, Condition::FULL_TENTHS)) /
		                     static_cast<float>(Condition::FULL_TENTHS);
		const auto sound = std::exp(-FALL_OFF);
		return a_multiple * (std::exp(-FALL_OFF * reading) - sound) / (1.0F - sound);
	}

	float Reached(float a_debt, float a_multiple)
	{
		// A debt that is no number, or a multiple of 0 or less, reads as
		// full.
		if (!(a_debt > 0.0F) || !(a_multiple > 0.0F)) {
			return static_cast<float>(FULL);
		}
		const auto sound = std::exp(-FALL_OFF);
		const auto reading = -std::log(a_debt * (1.0F - sound) / a_multiple + sound) / FALL_OFF;
		return std::clamp(reading * static_cast<float>(FULL), 0.0F, static_cast<float>(FULL));
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
		return "";
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
