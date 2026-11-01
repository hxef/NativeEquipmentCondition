#pragma once

#include "UI/Roles/Roles.h"

#include <optional>

// The HUD's pickup prompt, shown while the crosshair rests on something you
// can take or use: the name, and the button hints under it, "E) Take". The
// name's field grows up from its bottom as a long name wraps, and the hints
// start a short gap below it.
namespace Roles::Rollover
{
	// The prompt's clip at vanilla's place, when it holds the name's field and
	// the hints. Nothing for a HUD replacer that moved or dropped it.
	[[nodiscard]] std::optional<Value> Find(Scaleform::GFx::Movie& a_movie);

	// The field the prompt prints the name in, or a value that is not a
	// display object when a_prompt has none.
	[[nodiscard]] Value Name(const Value& a_prompt);

	// The bar of button hints under the name, the same way.
	[[nodiscard]] Value Hints(const Value& a_prompt);
}
