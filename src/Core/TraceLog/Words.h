#pragma once

#include "Core/Plugin.h"

#include <format>
#include <string>
#include <string_view>

// How a line names an actor and a damage type. TraceLog.h includes it, and
// every other file includes TraceLog.h for it.
namespace TraceLog
{
	// An actor in a line: the name the player sees and the form ID in brackets,
	// since a fight is often several actors with 1 name. The name is looked up
	// as the line is written, so a switched off line never calls into the
	// engine for it.
	struct Who
	{
		const RE::TESForm* form{ nullptr };
	};

	namespace detail
	{
		[[nodiscard]] std::string Name(const RE::TESForm* a_form);
	}

	// A damage type in a line. It has no name, and the game keeps no editor
	// ID for most forms unless another DLL mod keeps them. The resistance it
	// is checked against keeps its own, such as EnergyResist.
	[[nodiscard]] std::string_view TypeName(const RE::BGSDamageType& a_type);
}

// Lets a TraceLog::Who stand in a format string wherever a name would.
template <>
struct std::formatter<TraceLog::Who> : std::formatter<std::string_view>
{
	auto format(const TraceLog::Who& a_who, std::format_context& a_context) const
	{
		return std::formatter<std::string_view>::format(TraceLog::detail::Name(a_who.form), a_context);
	}
};
