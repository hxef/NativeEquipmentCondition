#include "Condition/Materials/Yield.h"

#include "Condition/Repair.h"

#include <algorithm>

namespace Materials
{
	std::uint32_t ScrapIn(std::uint32_t a_count, std::uint32_t a_percent)
	{
		// Whole numbers, never a float: 50 at 53% is 26.5 and rounds up to
		// 27, but 50 times 0.53F is 26.499998 and would round down to 26.
		const auto percent = std::min(a_percent, Repair::FULL);
		return static_cast<std::uint32_t>((std::uint64_t{ a_count } * percent + 50) / 100);
	}
}
