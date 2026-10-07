#include "Core/CallPatch/Ledger.h"

#include "Core/Feature.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Reading every place NEC noted back, since a DLL that patches after NEC can
// only be seen then: a place NEC wrote, a place NEC runs on top of, which a
// third DLL can write over, and a place NEC left alone as its row or its set
// did not go in. A place changed since is walked, see Walk.cpp. While the call
// still reaches NEC the 2 share the place, except in a set that has to run on
// every call, else NEC's change there is off from now on. The DLL that cuts
// NEC off is named, never one in front of it that hands the call on, but where
// NEC has to run on every call each DLL over it is named.
namespace CallPatch
{
	namespace
	{
		// The newest owner of a place, as a list of 1 for Words.
		std::string Newest(const Place& a_place)
		{
			return Words(std::span{ &a_place.owners.back(), 1 });
		}

		// The DLLs over NEC the walk found, outermost first, for the share,
		// cut and again lines: 1 name, or "B.dll over A.dll" for 2 or more.
		// A place no walk read names its newest owner, or else whoever it
		// leads to now.
		std::string By(const Place& a_place)
		{
			if (!a_place.line.empty()) {
				return Over(a_place.line);
			}
			if (!a_place.owners.empty()) {
				return Newest(a_place);
			}
			const auto owner = OwnerAt(a_place.where, a_place.kind, true);
			return Words(std::span{ &owner, 1 });
		}

		// Adds an owner to a place once. True when it is new.
		bool AddOwner(Place& a_place, const Owner& a_owner)
		{
			if (std::ranges::find(a_place.owners, a_owner) != a_place.owners.end()) {
				return false;
			}
			a_place.owners.push_back(a_owner);
			return true;
		}

		// The DLL that cuts NEC off on a line that does not reach NEC: the
		// last one the walk named, which keeps the game's own function or
		// nothing NEC can read. A DLL in front of it hands the call on and is
		// never named. With none named, whoever the place leads to.
		Owner Cutter(const Place& a_place, const Walked& a_walked)
		{
			return a_walked.dlls.empty() ? OwnerAt(a_place.where, a_place.kind, true) : a_walked.dlls.back();
		}

		// The sentence that ends the line of a place that stays off, or whose
		// row's switch is left to another mod.
		std::string Tail(const Place& a_place)
		{
			const auto trace = TraceTail(a_place);
			return std::string{ trace.empty() ? " NEC still counts it as left to another mod while the game runs."sv : trace };
		}

		// Whether NEC's change at a place does nothing while the game runs,
		// whatever stands on the place, since its row's switch is left to
		// another mod. A trace only place feeds the trace logs either way.
		bool SwitchLeft(const Place& a_place)
		{
			return a_place.use != Use::kTraceOnly && a_place.row->on && YieldedLocked(a_place.row->on);
		}

		// The line of a written place another DLL changed: the DLLs over NEC,
		// then a_tail and a_trace, what that leaves of NEC's change there.
		std::string ChangedLine(const Place& a_place, std::string_view a_tail, std::string_view a_trace = {})
		{
			return std::format("{:s}: {:s} at {:X} was changed by {:s} after NEC patched it.{:s}{:s}", NameOf(a_place), a_place.what,
				a_place.where, By(a_place), a_tail, a_trace);
		}

		// The line of a place NEC is off at from now on.
		std::string CutLine(const Place& a_place, bool a_everyCall)
		{
			return ChangedLine(a_place, a_place.use == Use::kTraceOnly ? " It only fed the bug report logs, so play is not affected."sv :
			                            a_everyCall ? " NEC's change there has to run on every call, so it is off from now on."sv :
			                                          " NEC's change there is off from now on."sv);
		}

		// Whether NEC.log has said a written place changed: one cut by its own
		// change has an owner, and every walk that reached NEC left its line.
		// A place off only since a cut elsewhere stopped its set has neither.
		bool Said(const Place& a_place)
		{
			return !a_place.owners.empty() || !a_place.line.empty();
		}

		// A place that leads straight to NEC's hook again. One that is off
		// stays off, since NEC never takes one back. A shared place is NEC's
		// again, and a held set that waited on it runs once more. True when it
		// gets a line, which only a place whose change got one does.
		bool Back(Place& a_place)
		{
			if (IsOff(a_place)) {
				return Said(a_place);
			}
			if (a_place.share != Share::kShared && a_place.share != Share::kProven) {
				return false;
			}
			a_place.share = a_place.under.empty() ? Share::kOwn : Share::kOnTop;
			a_place.line.clear();
			Release(a_place);
			return true;
		}
	}

	bool Recheck(std::string_view a_moment)
	{
		Lines       lines;
		std::size_t checked = 0;
		std::size_t changed = 0;
		const auto  before = Summary();
		{
			const std::scoped_lock    l{ Lock() };
			std::vector<const Place*> held;   // a held set's place the call still reaches, worded after the pass
			std::vector<const Place*> lone;   // a lone or switched place the call still reaches, worded after the pass
			std::vector<const Place*> late;   // changed after NEC left it alone
			std::vector<const Place*> again;  // already off, changed again
			std::vector<const Place*> back;   // back to NEC's hook
			Pass                      pass;
			for (auto& place : Places()) {
				checked++;

				const auto* now = reinterpret_cast<const void*>(place.where);
				if (std::memcmp(now, place.seen.data(), place.watch) == 0) {
					continue;
				}
				std::memcpy(place.seen.data(), now, place.watch);
				changed++;

				// Back as NEC wrote it.
				if (place.size != 0 && std::memcmp(now, place.wrote.data(), place.size) == 0) {
					if (Back(place)) {
						back.push_back(&place);
					}
					continue;
				}

				// A place that is off, or that NEC never wrote, and that leads
				// into the game again names nobody new.
				if ((IsOff(place) || place.size == 0) && LeadsIntoGame(place.where, place.kind)) {
					continue;
				}

				// A place NEC never wrote names every DLL that changes it,
				// since NEC had no change there to lose. One NEC left alone
				// gets the again line, any other a line of its own.
				if (place.size == 0) {
					if (AddOwner(place, OwnerAt(place.where, place.kind, true))) {
						(IsOff(place) ? again : late).push_back(&place);
					}
					continue;
				}

				// A place NEC wrote, changed by another DLL. Follow the line.
				const auto walked = Walk(place);
				const auto reaches = walked.reaches == Reaches::kNec;

				// Stubs alone that lead straight to NEC's hook, as a DLL that
				// unhooks with a fresh stub of its own leaves the place. NEC's
				// hook runs on every call there, as if NEC had written it.
				if (reaches && walked.dlls.empty()) {
					if (Back(place)) {
						back.push_back(&place);
					}
					continue;
				}

				// Read before the walk's line goes in, see Said.
				const auto first = !Said(place);
				place.line = walked.dlls;

				// A set that has to run on every call, see
				// CallPatch::EVERY_CALL, goes to every DLL over NEC there for
				// good. Elsewhere a line that skips NEC is named after the DLL
				// that cuts NEC off.
				const auto everyCall = EveryCall(place.set);
				if (everyCall) {
					for (const auto& dll : walked.dlls) {
						AddOwner(place, dll);
					}
				}
				if (!reaches) {
					AddOwner(place, Cutter(place, walked));
				}

				// Off for good already. Its first change reads as left to
				// another mod where the call still reaches NEC and the set need
				// not run on every call, else as a cut. Any later one reads as
				// changed again.
				if (place.share == Share::kCut) {
					if (first) {
						lines.push_back({ REX::ELogLevel::Warning,
							reaches && !everyCall ? ChangedLine(place, Tail(place)) : CutLine(place, everyCall) });
					} else {
						again.push_back(&place);
					}
					continue;
				}

				if (everyCall || !reaches) {
					Lose(place, pass, Share::kCut);
					lines.push_back({ REX::ELogLevel::Warning, CutLine(place, everyCall) });
				} else if (HeldSet(place.set)) {
					// A held set waits until this place's hook runs again.
					// Hold fails only when a sibling already cut the set, which
					// never comes back, so this place is off with it and names
					// nobody. Worded once the pass is over.
					if (Hold(place)) {
						place.share = Share::kShared;
					} else {
						Lose(place, pass, Share::kCut);
					}
					held.push_back(&place);
				} else {
					// A lone or switched place keeps working while the call
					// still reaches NEC. Worded once the pass is over.
					place.share = Share::kShared;
					lone.push_back(&place);
				}
			}

			// A written place whose set a cut stopped is off with it, whatever
			// the DLL over it does, and names nobody. Set before any line
			// below is worded, so each line reads how the pass ended.
			for (auto& place : Places()) {
				if (place.size != 0 && !IsOff(place) && RunOf(place.set) == Run::kOff) {
					place.share = Share::kCut;
				}
			}
			// Worded once every place of the pass is lost, so a place checked
			// before its row's main place still reads the switch that place
			// turns off.
			for (const auto* place : held) {
				const auto off = PiecesOffTail(*place);
				// A held place is off only since a sibling's cut, as its own
				// walk reached NEC, and that cut's line comes first.
				lines.push_back({ REX::ELogLevel::Warning,
					IsOff(*place) || SwitchLeft(*place) ? ChangedLine(*place, Tail(*place)) :
					!off.empty()                        ? ChangedLine(*place, off) :
					                                      ChangedLine(*place, " If the call still reaches NEC, NEC's change there comes back the next time it is used."sv, TraceTail(*place)) });
			}
			for (const auto* place : lone) {
				const auto off = PiecesOffTail(*place);
				lines.push_back({ REX::ELogLevel::Warning,
					SwitchLeft(*place) ? ChangedLine(*place, Tail(*place)) :
					!off.empty()       ? ChangedLine(*place, off) :
					                     ChangedLine(*place, " NEC's change there keeps working as long as the call still reaches NEC."sv, TraceTail(*place)) });
			}
			// A held set put to wait that has run since comes back on. Read
			// once the pass is over, so a set this pass made wait or stopped
			// never reads as back on, and none is said in a row whose switch
			// is left to another mod or for a place whose pieces are all off
			// anyway.
			for (auto& place : Places()) {
				if (HeldSet(place.set) && place.share == Share::kShared && RunOf(place.set) == Run::kLive) {
					place.share = Share::kProven;
					if (!SwitchLeft(place) && PiecesOffTail(place).empty()) {
						lines.push_back({ REX::ELogLevel::Info,
							std::format("{:s}: NEC's change at {:s} runs again.", NameOf(place), place.what) });
					}
				}
			}
			for (const auto* place : late) {
				lines.push_back({ REX::ELogLevel::Warning,
					std::format("{:s}: {:s} at {:X} was changed by {:s} after NEC left it alone.{:s}", NameOf(*place), place->what,
						place->where, Newest(*place), TraceTail(*place)) });
			}
			for (const auto* place : again) {
				lines.push_back({ REX::ELogLevel::Warning,
					std::format("{:s}: {:s} at {:X} was changed again, now by {:s}.{:s}", NameOf(*place), place->what, place->where,
						By(*place), TraceTail(*place)) });
			}
			for (const auto* place : back) {
				lines.push_back({ REX::ELogLevel::Warning,
					std::format("{:s}: {:s} at {:X} is back as NEC wrote it.{:s}", NameOf(*place), place->what, place->where,
						IsOff(*place) || SwitchLeft(*place) ? Tail(*place) : TraceTail(*place)) });
			}
			Say(pass, lines, false);
		}
		Write(lines);

		if (changed == 0) {
			REX::DEBUG("Checked {:d} places again as {:s}, none changed.", checked, a_moment);
		} else {
			REX::WARN("Checked {:d} places again as {:s}, {:d} changed.", checked, a_moment, changed);
		}
		return changed != 0 && Summary() != before;
	}
}
