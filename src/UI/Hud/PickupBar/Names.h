#pragma once

#include "Core/Plugin.h"

#include <string_view>

// Telling whether the pickup prompt shows the item NEC read. Private to this
// folder.
namespace PickupBar
{
	// Whether the prompt's a_text names a_name. The prompt prints the name
	// the game built, in capitals and with a count after it for a pile of
	// several, so the name only has to be in it once both are in capitals.
	// Some prompts print the name without a sorter's tag, such as [10mm], so
	// the name without it counts too, but only as the whole text or with a
	// count after it, so another item never matches.
	[[nodiscard]] bool Names(std::string_view a_text, std::string_view a_name);
}
