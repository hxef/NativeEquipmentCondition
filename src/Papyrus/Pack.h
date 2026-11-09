#pragma once

#include "Core/Plugin.h"

#include "Papyrus/Items.h"

// An event's arguments, each packed as exactly the type the callback
// declares. The VM hands an event to a callback only when every argument's
// type equals its parameter's. PackAs is here since the event's kind picks
// the holder's declared type at run time, Actor for Jammed and
// ObjectReference for the rest, and the fork's packer picks by C++ type.
// Private to this folder.
namespace Papyrus
{
	// a_form as an object of the script type a_type: the one bound to it, or
	// one made and bound, as the engine's own packer does for an
	// ObjectReference. None for no form and no VM.
	[[nodiscard]] RE::BSScript::Variable PackAs(const RE::TESForm* a_form, RE::BSScript::ENUM_TYPE_ID a_type);

	// An item as an ObjectReference: a reference through PackAs, a stack's
	// handle through the fork's packer. None for None. A handle is packed
	// under its holder's inventory lock, see Events.cpp.
	[[nodiscard]] RE::BSScript::Variable PackItem(const Item& a_item);
}
