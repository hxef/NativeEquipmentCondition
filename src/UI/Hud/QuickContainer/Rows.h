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

	// The condition a row holds for an item that neither wears out nor spoils.
	inline constexpr std::int32_t NO_CONDITION = -1;

	// Patches the calls that build the rows. Returns whether both succeeded.
	bool PatchRows();

	// The condition behind a row: a whole percent, or NO_CONDITION, and the
	// tenths its card writes, see Condition::CardTenths, 0 with no
	// condition.
	struct Reading
	{
		std::int32_t  percent{ NO_CONDITION };
		std::uint32_t tenths{ 0 };

		bool operator==(const Reading&) const = default;
	};

	// What one row on screen shows.
	struct Shown
	{
		std::string   text;
		std::uint32_t count{ 0 };
	};

	// The conditions for the rows on screen, from the newest kept build whose
	// rows read the same, or nothing when none of them does.
	std::optional<std::array<Reading, MAX_ROWS>> FindConditions(const std::array<Shown, MAX_ROWS>& a_shown, std::size_t a_size);

	// Whether the container of the newest build is a fridge, see
	// FoodSpoil::IsFridgeRef. The HUD shows that build a frame or so later.
	[[nodiscard]] bool ShowsFridge();
}
