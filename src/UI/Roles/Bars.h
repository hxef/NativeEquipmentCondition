#pragma once

#include "UI/Roles/Roles.h"

#include <optional>

// The button hints a menu shows, wherever its movie keeps them: in the list
// the engine's bar holds, or in a bar of the movie's own that answers a method
// in Conventions.h, found by a walk under menuObj at each call, at most 3 deep,
// since bars can share a name and come late. Each list is an AS3 Vector, read
// and changed only through its length, push, indexOf and forEach.
namespace Roles::Conventions
{
	struct Bar;
}

namespace Roles::Bars
{
	struct Hints
	{
		Value                   list;                  // the hints the bar draws now
		Value                   bar;                   // told to draw again after a change
		const Conventions::Bar* convention = nullptr;  // the methods it answered, none for the engine's bar
		bool                    ours = false;          // the list holds a_ours already
	};

	// The list a_ours belongs in: the one holding it already, else the
	// engine's while it holds any hint, else the bar of the movie's own
	// holding the most hints, a_ours not counted. Hidden hints count too,
	// since a quantity box or a question hides them all for a while. Nothing
	// when no list holds a hint or 2 bars of the movie's own tie.
	[[nodiscard]] std::optional<Hints> InUse(RE::GameMenuBase& a_menu, const Value& a_ours);

	// Adds a_hint last and has the bar draw again, the engine's by handing it
	// its list back. False when the list refuses.
	bool Add(Hints& a_hints, Value& a_hint);

	// Calls a_visit with every hint of every bar the menu has, so a word is
	// written wherever its hint shows.
	void ForEach(RE::GameMenuBase& a_menu, Scaleform::GFx::FunctionHandler& a_visit);
}
