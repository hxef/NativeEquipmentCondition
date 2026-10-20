#include "Core/CallPatch/Changed.h"
#include "Core/CallPatch/MenuWait.h"

#include "Core/Feature.h"

#include <algorithm>
#include <format>
#include <span>
#include <string>
#include <string_view>

// The NEC.log lines a place another DLL changed gets, and the DLL each one
// names. They run under the ledger's lock, as Recheck.cpp calls them.
namespace CallPatch
{
	namespace
	{
		// The sentence that says NEC could not read the walk, see CutLine.
		std::string CannotRead(const Place& a_place, const Walked& a_walked)
		{
			const auto cutter = Cutter(a_place, a_walked);
			return std::format(" NEC cannot read where {:s}'s hook leads, so it cannot tell whether the call still reaches NEC.",
				Words(std::span{ &cutter, 1 }));
		}
	}

	std::string Newest(const Place& a_place)
	{
		return Words(std::span{ &a_place.owners.back(), 1 });
	}

	std::string By(const Place& a_place)
	{
		const auto* wait = WaitingOf(a_place);
		const auto& line = wait ? wait->line : a_place.line;
		if (!line.empty()) {
			return Over(line);
		}
		if (!a_place.owners.empty()) {
			return Newest(a_place);
		}
		const auto owner = OwnerAt(a_place.where, a_place.kind, true);
		return Words(std::span{ &owner, 1 });
	}

	bool AddOwner(Place& a_place, const Owner& a_owner)
	{
		if (std::ranges::find(a_place.owners, a_owner) != a_place.owners.end()) {
			return false;
		}
		a_place.owners.push_back(a_owner);
		return true;
	}

	Owner Cutter(const Place& a_place, const Walked& a_walked)
	{
		return a_walked.dlls.empty() ? OwnerAt(a_place.where, a_place.kind, true) : a_walked.dlls.back();
	}

	std::string Tail(const Place& a_place)
	{
		const auto trace = TraceTail(a_place);
		return std::string{ trace.empty() ? " NEC still counts it as left to another mod while the game runs."sv : trace };
	}

	bool SwitchLeft(const Place& a_place)
	{
		return a_place.use != Use::kTraceOnly && a_place.row->on && YieldedLocked(a_place.row->on);
	}

	std::string ChangedLine(const Place& a_place, std::string_view a_tail, std::string_view a_trace)
	{
		return std::format("{:s}: {:s} at {:X} was changed by {:s} after NEC patched it.{:s}{:s}", NameOf(a_place), a_place.what,
			a_place.where, By(a_place), a_tail, a_trace);
	}

	std::string CutLine(const Place& a_place, bool a_everyCall, const Walked& a_walked)
	{
		std::string tail;
		if (!a_everyCall && a_walked.reaches == Reaches::kUnread) {
			tail = CannotRead(a_place, a_walked);
		}
		if (a_place.part == Part::kBenchMessages) {
			const auto off = PiecesOffTail(a_place);
			tail += off.empty() ? " NEC's 2 messages when MODIFY opens nothing show only while the call reaches NEC, so repairs are not affected."sv : std::string_view{ off };
		} else if (a_place.part == Part::kTraderKeys) {
			const auto off = PiecesOffTail(a_place);
			tail += off.empty() ? " REPAIR's key may answer a UI replacer's own button combinations again, so trader repairs are not affected."sv : std::string_view{ off };
		} else {
			tail += a_place.use == Use::kTraceOnly ? " It only fed the bug report logs, so play is not affected."sv :
			        a_everyCall                    ? " NEC's change there has to run on every call, so it is off from now on."sv :
			                                         " NEC's change there is off from now on."sv;
		}
		return ChangedLine(a_place, tail);
	}

	std::string LoopLine(const Place& a_place, const Owner& a_dll)
	{
		const auto dll = Words(std::span{ &a_dll, 1 });
		return std::format("{:s}: {:s} hooked {:s} at {:X} both before and after NEC, so its hook and NEC's hand each call back and forth without end. The game will most likely crash the next time it is used, and removing {:s} stops that.",
			NameOf(a_place), dll, a_place.what, a_place.where, dll);
	}

	// A waiting place keeps only the DLL that cuts NEC off in its line, see
	// MenuWait.
	std::string WaitTail(const Place& a_place)
	{
		const auto* wait = WaitingOf(a_place);
		if (!wait) {
			return std::string{ " If the call still reaches NEC, NEC's change there comes back the next time it is used."sv };
		}
		const auto doubt = wait->unread ? CannotRead(a_place, { .dlls = a_place.line }) : std::string{};
		return std::format("{:s} NEC's change there comes back if the call reaches NEC the next time the {:s} opens, and stays off for good if it does not.",
			doubt, MenuWords(*wait));
	}

	std::string VerdictLine(const Place& a_place, const MenuWait& a_wait)
	{
		const auto tail = std::format(" The {:s} opened and the call did not reach NEC, so NEC's change there is off from now on.", MenuWords(a_wait));
		return ChangedLine(a_place, tail);
	}
}
