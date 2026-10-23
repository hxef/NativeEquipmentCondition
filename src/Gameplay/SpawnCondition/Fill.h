#pragma once

#include "Core/Plugin.h"

#include "Gameplay/SpawnCondition/Roll.h"

#include <cstdint>
#include <optional>

// A list filled from a container's record: the scope that counts what its
// AddStack calls roll, the lines with no mod slots, and the stock record.
// Private to this folder.
namespace SpawnCondition
{
	// Roll, counted for the fill in progress on this thread when it aims or
	// writes a health, see FillHk. Outside a fill it is Roll.
	std::optional<Aim> RollInFill(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::ExtraDataList* a_extra,
		std::uint32_t a_count, bool a_private);

	// The same for a stack that is already built.
	std::optional<Aim> RollInFill(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack);

	// Patches the list fill: the lines with no mod slots, and the stock record.
	void InstallFill();
}
