#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// What a worn item owes, before deciding what it is paid in. An item at a
// condition owes a share of its worth, all of it at 0 and none at 100, and a
// repair pays the difference between what it owes now and what it will owe
// after. Only those 2 conditions count, so 1 repair from 20 to 100 costs the
// same as 8 repairs of 10 each.
//
// The workbench asks for components and a trader for caps, so the curve, the
// steps and the rounding live here once.
namespace Repair
{
	// Full condition.
	inline constexpr std::uint32_t FULL = 100;

	// The levels a worn item can be repaired to. Only the ones above its
	// condition are offered, so a gun at 95 only gets 100 and a broken one gets
	// all 10. Small steps let a player short of components pay for half a
	// repair now and the rest later.
	inline constexpr std::uint32_t LEVELS[]{ 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };

	// Numbers, not words, so they read the same in every language.
	inline constexpr const char* LEVEL_NAME[]{ "10%", "20%", "30%", "40%", "50%",
		"60%", "70%", "80%", "90%", "100%" };

	// How sharply the price falls as the item nears full condition. At 0 every
	// point of condition costs the same. At 1.75 a broken item owes 100%, a gun
	// at 10% owes 81%, at 50% owes 29% and at 90% owes 4%, so the first 10
	// points cost 5 times the last 10. Any steeper and the low levels cost so
	// much they stop being worth offering.
	inline constexpr float FALL_OFF = 1.75F;

	// What an item at a condition still owes, as a share of a_multiple: all of
	// it at 0, none at 100, the curve above in between. The curve is e to the
	// minus k x, shifted and scaled so both ends land exactly, so a_multiple
	// and FALL_OFF can be tuned separately.
	[[nodiscard]] float Debt(std::uint32_t a_percent, float a_multiple);

	// Components and caps come in whole units. Half of one rounds up.
	[[nodiscard]] std::uint32_t Whole(float a_units);

	// The condition levels above the one an item already has.
	[[nodiscard]] std::vector<std::uint32_t> Above(std::uint32_t a_percent);

	// What a level is called on a button, or nothing for a number that is not a
	// level.
	[[nodiscard]] const char* NameOf(std::uint32_t a_level);

	// One level an item can be brought back to and its price, in whatever the
	// price is counted in.
	template <class Price>
	struct Step
	{
		std::uint32_t level;
		Price         price;
	};

	// The steps worth offering: every one whose price differs from the next,
	// and the top always. On a cheap item several steps round to the same
	// handful of units, and paying the same for less is never wanted.
	template <class Price>
	[[nodiscard]] std::vector<Step<Price>> Distinct(std::vector<Step<Price>> a_steps)
	{
		std::vector<Step<Price>> out;
		for (std::size_t i = 0; i < a_steps.size(); i++) {
			if (i + 1 == a_steps.size() || a_steps[i].price != a_steps[i + 1].price) {
				out.push_back(std::move(a_steps[i]));
			}
		}
		return out;
	}

	// What an item owes at each level against a_multiple, as "10%:1.61
	// 20%:1.29 ...", for the log.
	[[nodiscard]] std::string Ladder(float a_multiple);
}
