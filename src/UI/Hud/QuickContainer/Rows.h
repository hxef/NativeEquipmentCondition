#pragma once

#include "Core/Plugin.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// The rows as the game builds them. Private to this folder.
namespace QuickContainer
{
	// The widget has 5 rows.
	inline constexpr std::size_t MAX_ROWS = 5;

	// The condition a row holds for an item that does not wear out.
	inline constexpr std::int32_t NO_CONDITION = -1;

	// Patches the calls that build the rows. Returns whether both succeeded.
	bool PatchRows();

	// What one row on screen shows.
	struct Shown
	{
		std::string   text;
		std::uint32_t count{ 0 };
	};

	// The conditions for the rows on screen, from the newest kept build whose
	// rows read the same, or nothing when none of them does.
	std::optional<std::array<std::int32_t, MAX_ROWS>> FindConditions(const std::array<Shown, MAX_ROWS>& a_shown, std::size_t a_size);
}
