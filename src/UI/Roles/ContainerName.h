#pragma once

#include "UI/Roles/Roles.h"

#include <optional>

// The container's name at the top of the HUD's quick container, the clip that
// holds it and the text field it prints in. The field sizes itself to the
// name and stays centred.
namespace Roles::ContainerName
{
	// The name's clip at vanilla's place, when it and its field are display
	// objects. Nothing for a HUD replacer that moved or dropped it.
	[[nodiscard]] std::optional<Value> Find(Scaleform::GFx::Movie& a_movie);

	// The field a_clip prints the name in, or a value that is not a display
	// object when it has none.
	[[nodiscard]] Value Field(const Value& a_clip);
}
