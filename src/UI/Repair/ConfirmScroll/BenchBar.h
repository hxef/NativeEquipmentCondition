#pragma once

#include "Core/Plugin.h"

#include <optional>

// Where the open bench's button bar is, so a grown box stays above it.
// Private to this folder.
namespace ConfirmScroll
{
	// How far down the screen the open bench's button bar starts, 0 to 1,
	// measured each time. Nothing when no bar is shown, and 1 trace line
	// says why.
	[[nodiscard]] std::optional<double> BenchBarTop();
}
