#pragma once

#include "UI/Roles/Roles.h"

#include <optional>

// The card the level up perk chart shows for the perk under the mouse: its
// name, the rank stars and the description. The chart puts the stars right
// under the description's last line, so the card always makes room for every
// line, while the field itself is 2 lines high.
namespace Roles::PerkCard
{
	// The field the card prints the perk's description in, at vanilla's
	// place, when it is a text field. Nothing for a chart replacer that moved
	// or dropped it.
	[[nodiscard]] std::optional<Value> Description(Scaleform::GFx::Movie& a_movie);
}
