#pragma once

#include "Core/Plugin.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// Asking how far to repair a worn item. The box is the game's own, headed
// REPAIR, with a button per condition on offer and a cancel button after them.
// The caller decides the line above the question: the bench names the perk, the
// trader says how far they can go. The answer comes back through F4SE's task
// queue, since the moment a box answers is not safe for spending anything or
// opening a menu, and the bench has crashed on work done at a moment Flash
// chose. The box pauses the game, and F4SE then runs its tasks on the main
// thread.
namespace RepairPrompt
{
	// Puts the question up. a_over is the line above it, or nothing. a_buttons
	// are the offers in order, and the cancel button is added after them.
	// a_chosen is handed the number of the offer picked, and a_cancelled runs
	// for cancel. Both run a moment later, from F4SE's task queue.
	void Ask(std::string_view a_over, std::string_view a_name, std::uint32_t a_percent,
		std::vector<std::string> a_buttons,
		std::function<void(std::size_t)> a_chosen, std::function<void()> a_cancelled = {});
}
