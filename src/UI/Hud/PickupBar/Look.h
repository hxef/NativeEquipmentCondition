#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <optional>
#include <string>

// The item the pickup prompt stands for, as the game builds the prompt.
// Private to this folder.
namespace PickupBar
{
	// Patches the call that builds the prompt. Returns whether it went in.
	bool PatchLook();

	// The item of the newest prompt: its name as the prompt prints it, and
	// its condition, or a food's freshness, as a whole percent.
	struct Looked
	{
		std::string         name;
		std::int32_t        percent{ 0 };
		RE::ObjectRefHandle item;
	};

	// The newest prompt's item, or nothing while it shows no weapon or armor
	// that wears and no food that spoils.
	[[nodiscard]] std::optional<Looked> Kept();
}
