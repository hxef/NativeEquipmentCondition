#pragma once

#include "Core/Plugin.h"

#include "Condition/Provenance/Provenance.h"
#include "Core/TraceLog.h"
#include "Gameplay/SpawnCondition/SpawnCondition.h"

#include <format>
#include <string>
#include <string_view>
#include <utility>

// What the trace log is told about a stack. Private to this folder.
namespace SpawnCondition
{
	// What to call something in the trace log. A generic guard and every
	// creature's weapon are nameless on purpose, and an empty name would
	// leave a hole in the line. An editor ID would read better but costs
	// another engine call under the inventory lock.
	std::string_view NameOf(const RE::TESForm& a_form, std::string_view a_whenNameless);

	// Which halves were measured, for the trace log, so a weapon at a
	// surprising condition shows whether the list, the owner or neither
	// caused it.
	std::string Describe(const Provenance::Origin& a_origin);

	// Whose stock an item is and the band it rolled in, for the trace log.
	std::string Stocked(const Restock& a_restock, const StockBand& a_band);

	// A stack for the trace log: a line of its own where it is the player's
	// or a trader's restock, and otherwise a count by what became of it, with
	// the line under the quiet loot tag for whoever needs it back, see
	// TraceLog.h.
	template <class... T>
	void Report(bool a_listed, std::string_view a_what, std::format_string<T...> a_fmt, T&&... a_args)
	{
		if (a_listed) {
			TraceLog::Line("spawn", a_fmt, std::forward<T>(a_args)...);
		} else {
			TraceLog::Count("spawn", a_what);
			TraceLog::Line("loot", a_fmt, std::forward<T>(a_args)...);
		}
	}
}
