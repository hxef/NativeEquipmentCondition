#pragma once

#include "Core/TraceLog/TraceLog.h"

#include <string_view>

// The Papyrus file's hold, which Hold.cpp keeps and TraceLog.cpp asks
// before it writes a line. Private to this folder.
namespace TraceLog::detail
{
	// True when the line went to its thread's list instead of the file, see
	// Papyrus::Hold.
	bool Kept(Target a_target, bool a_begin, std::string_view a_tag, std::string_view a_text);
}
