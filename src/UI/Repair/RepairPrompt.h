#pragma once

#include "Core/Plugin.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Asking how far to repair a worn item. The box is a vanilla one, headed
// REPAIR, with a button per condition on offer, the highest first, and a cancel
// button after them.
// The caller decides the line above the question: the bench names the perk, the
// trader says how far they can go. The answer comes back through F4SE's task
// queue, since the moment a box answers is not safe for spending anything or
// opening a menu, and work done at a moment Flash chose can crash the bench.
// The box pauses the game, and F4SE then runs its tasks on the main thread.
namespace RepairPrompt
{
	// The vanilla refusal sound in menus, the one the box's cancel button
	// plays. The vanilla SoundMenuCancel has no sound file.
	inline constexpr const char* REFUSED_SOUND = "UIMenuCancel";

	// A row above the levels and what it does, run from the task queue.
	struct Top
	{
		std::string           label;
		std::function<void()> chosen;
	};

	// Puts the question up. a_over is the line above it, or nothing. a_buttons
	// are the offers lowest first. The box lists them highest first, so full
	// condition needs no scrolling, and adds the cancel button after them.
	// a_chosen is handed the offer's number in a_buttons, and a_cancelled runs
	// for cancel. Both run a moment later, from F4SE's task queue. a_top goes
	// above the highest offer, and the highlight still starts on that offer.
	void Ask(std::string_view a_over, std::string_view a_name, std::uint32_t a_percent,
		std::vector<std::string> a_buttons,
		std::function<void(std::size_t)> a_chosen, std::function<void()> a_cancelled = {},
		std::optional<Top> a_top = std::nullopt);

	// Puts a box headed REPAIR up with a_body over a_rows in their order and
	// the cancel button after them. a_chosen is handed the row's number, from
	// the task queue like a_cancelled. The highlight starts on row a_start.
	void AskRows(std::string_view a_body, std::vector<std::string> a_rows,
		std::function<void(std::size_t)> a_chosen, std::function<void()> a_cancelled = {}, std::uint32_t a_start = 0);
}
