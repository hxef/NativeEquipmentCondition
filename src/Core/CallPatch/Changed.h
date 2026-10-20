#pragma once

#include "Core/CallPatch/Ledger.h"

#include <string>
#include <string_view>

// What NEC.log says about a place another DLL changed after NEC noted it, for
// Recheck.cpp. Private to this folder.
namespace CallPatch
{
	struct MenuWait;  // MenuWait.h

	// The newest owner of a place, as a list of 1 for Words.
	[[nodiscard]] std::string Newest(const Place& a_place);

	// The DLLs over NEC the walk found, outermost first, for the share,
	// cut, again, wait and verdict lines: 1 name, or "B.dll over A.dll" for
	// 2 or more. A place that waits for its menu takes them from its wait,
	// see MenuWait. A place no walk read names its newest owner, or else
	// whoever it leads to now.
	[[nodiscard]] std::string By(const Place& a_place);

	// Adds an owner to a place once. True when it is new.
	bool AddOwner(Place& a_place, const Owner& a_owner);

	// The DLL that cuts NEC off on a line that does not reach NEC: the
	// last one the walk named, which keeps the game's own function or
	// nothing NEC can read. A DLL in front of it hands the call on and is
	// never named. With none named, whoever the place leads to.
	[[nodiscard]] Owner Cutter(const Place& a_place, const Walked& a_walked);

	// The sentence that ends the line of a place that stays off, or whose
	// row's switch is left to another mod.
	[[nodiscard]] std::string Tail(const Place& a_place);

	// Whether NEC's change at a place does nothing while the game runs,
	// whatever stands on the place, since its row's switch is left to
	// another mod. A place from Part::kTrace on works the same either way.
	[[nodiscard]] bool SwitchLeft(const Place& a_place);

	// The line of a written place another DLL changed: the DLLs over NEC,
	// then a_tail and a_trace, what that leaves of NEC's change there.
	[[nodiscard]] std::string ChangedLine(const Place& a_place, std::string_view a_tail, std::string_view a_trace = {});

	// The line of a place NEC is off at from now on. The bench place's
	// hook asks nothing of its place, so its 2 messages still show
	// whenever the call reaches it, unless Workbench repairs is off. Every
	// place that turns repairs off comes before it, see Workbench.cpp, so
	// its line reads a cut in the same pass too. The barter input place's
	// key rule stops with its hook, and does nothing while Trader repairs
	// is off, whose 4 places come before it, see VendorRepair.cpp. A barter
	// place that waits for its screen is settled after this line, so the
	// line reads Trader repairs as on. A walk NEC could not follow to its
	// end says so first, so the line never blames the DLL for a cut that
	// comes from NEC's doubt. A set that has to run on every call is cut
	// either way.
	[[nodiscard]] std::string CutLine(const Place& a_place, bool a_everyCall, const Walked& a_walked);

	// The line of a place whose hooks call each other without end, see
	// Walked. The game runs out of stack at the next call whatever NEC
	// does there, so the line names the DLL to remove.
	[[nodiscard]] std::string LoopLine(const Place& a_place, const Owner& a_dll);

	// The sentence that ends the line of a place that waits for its hook to
	// run: when NEC's change there comes back, and for a place that waits
	// for its menu, when it goes off for good, after the sentence about any
	// doubt in the walk.
	[[nodiscard]] std::string WaitTail(const Place& a_place);

	// The line of a place that waited for its menu, which opened and closed
	// with no call reaching NEC.
	[[nodiscard]] std::string VerdictLine(const Place& a_place, const MenuWait& a_wait);
}
