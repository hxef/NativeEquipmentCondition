#pragma once

#include "Core/Plugin.h"

#include <cstdint>

// The number the engine gives 1 stack inside a container so a script can name
// it: ExtraUniqueID on the stack's extra data list, with the form ID of the
// container that gave it. 0 is no number. The engine numbers a stack when a
// script asks for a persistent item inside a container, and again when a
// numbered stack moves into another one, and takes the number off once no
// script holds it. A split copies the list, number and all, so 2 stacks of 1
// container can hold 1 number, and every lookup of the engine then takes the
// first in list order.
namespace UniqueID
{
	// The stack's number, 0 for none or no list.
	[[nodiscard]] std::uint16_t Of(const RE::ExtraDataList* a_extra);

	// Writes a_uid onto a_extra with a_holder as the container that gave it,
	// the engine's own setter. Run it inside a write functor of the stack,
	// under the holder's inventory lock.
	void Give(RE::ExtraDataList& a_extra, std::uint16_t a_uid, const RE::TESObjectREFR& a_holder);

	// Takes the number off a_extra, the same setter with nothing to write.
	void Clear(RE::ExtraDataList& a_extra);
}
