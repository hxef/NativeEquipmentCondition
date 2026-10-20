#include "Core/CallPatch/Changed.h"
#include "Core/CallPatch/MenuWait.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Reading every place NEC noted back, since a DLL that patches after NEC can
// only be seen then: a place NEC wrote, a place NEC runs on top of, which a
// third DLL can write over, and a place NEC left alone as its row or its set
// did not go in. A place changed since is walked, see Walk.cpp. While the call
// still reaches NEC the 2 share the place, except in a set that has to run on
// every call, else NEC's change there is off from now on. The DLL that cuts
// NEC off is named, never one in front of it that hands the call on, but where
// NEC has to run on every call each DLL over it is named. A place MenuWait.h
// lists waits for its menu instead.
namespace CallPatch
{
	namespace
	{
		// Whether NEC.log has said a written place changed: one cut by its own
		// change has an owner, and every walk that reached NEC left its line.
		// A place off only since a cut elsewhere stopped its set, with no
		// change of its own, has neither.
		bool Said(const Place& a_place)
		{
			return !a_place.owners.empty() || !a_place.line.empty();
		}

		// A place that leads straight to NEC's hook again. One that is off
		// stays off, since NEC never takes one back. A shared place is NEC's
		// again, and a set that waited on it runs once more. True when it
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
			if (auto* wait = MenuWaitOf(a_place)) {
				Done(*wait);
			}
			Release(a_place);
			return true;
		}

		// A menu's place the call may not reach, settled once every cut of
		// the pass is in.
		struct Waiting
		{
			Place*    place;
			MenuWait* wait;
			Walked    walked;
		};

		// A place that waited for its menu, which opened and closed with no
		// call reaching NEC's hook, is off from now on. Judged before the pass
		// reads any change, so a hook put in after the menu closed never saves
		// the one that skipped NEC, and every later check of the pass sees the
		// cut and the switch it turned off. True when one is cut.
		bool Judge(Pass& a_pass, Lines& a_lines)
		{
			auto judged = false;
			for (auto& place : Places()) {
				auto* wait = WaitingOf(place);
				if (wait && place.share == Share::kShared && MenuUsed(*wait) && Proving(place)) {
					place.line = Done(*wait);
					AddOwner(place, Cutter(place, { .dlls = place.line }));
					Lose(place, a_pass, Share::kCut);
					a_lines.push_back({ REX::ELogLevel::Warning, VerdictLine(place, *wait) });
					judged = true;
				}
			}
			return judged;
		}

		// A wait gains nothing while the row's switch is left to another mod
		// or the place's pieces are off for good, so such a place is cut like
		// any other. A place that waits names only the DLL that cuts NEC off
		// on the page and in its part's line, and its wait keeps the walk's
		// DLLs for its own line, see By.
		void Settle(std::span<const Waiting> a_waits, Pass& a_pass, Lines& a_lines, std::vector<const Place*>& a_held)
		{
			for (const auto& [place, wait, walked] : a_waits) {
				if (SwitchLeft(*place) || !PiecesOffTail(*place, true).empty()) {
					AddOwner(*place, Cutter(*place, walked));
					Lose(*place, a_pass, Share::kCut);
					a_lines.push_back({ REX::ELogLevel::Warning, CutLine(*place, false, walked) });
					continue;
				}
				// Never false, as the place is not off and its set has only it.
				Hold(*place);
				place->share = Share::kShared;
				Wait(*wait, std::exchange(place->line, std::vector<Owner>{ Cutter(*place, walked) }),
					walked.reaches == Reaches::kUnread);
				a_held.push_back(place);
			}
		}
	}

	bool Recheck(std::string_view a_moment)
	{
		Lines       lines;
		std::size_t checked = 0;
		std::size_t changed = 0;
		bool        judged = false;
		const auto  before = Summary();
		{
			const std::scoped_lock    l{ Lock() };
			std::vector<const Place*> held;   // a place of a set that waits, worded after the pass
			std::vector<const Place*> lone;   // a lone or switched place the call still reaches, worded after the pass
			std::vector<const Place*> late;   // changed after NEC left it alone
			std::vector<const Place*> again;  // already off, changed again
			std::vector<const Place*> back;   // back to NEC's hook
			std::vector<std::string>  loops;  // hooks that call each other without end
			std::vector<Waiting>      waits;
			Pass                      pass;

			judged = Judge(pass, lines);

			for (auto& place : Places()) {
				checked++;

				const auto* now = reinterpret_cast<const void*>(place.where);
				if (std::memcmp(now, place.seen.data(), place.watch) == 0) {
					continue;
				}
				std::memcpy(place.seen.data(), now, place.watch);
				// Counted for the line below, and taken back out for a
				// change that gets no line, so the line counts only what
				// NEC.log names. A change with no line leaves everything the
				// summary reads as it was, so the answer at the end stays
				// right.
				changed++;

				// Back on NEC's hook. A place whose change NEC.log never named,
				// NEC's own or off only since another place's cut stopped its
				// set, gets no line now either, see Back.
				const auto returned = [&]() {
					if (Back(place)) {
						back.push_back(&place);
					} else {
						changed--;
					}
				};

				// Back as NEC wrote it.
				if (place.size != 0 && std::memcmp(now, place.wrote.data(), place.size) == 0) {
					returned();
					continue;
				}

				// A place that is off, or that NEC never wrote, and that leads
				// into the game again names nobody new and gets no line.
				if ((IsOff(place) || place.size == 0) && LeadsIntoGame(place.where, place.kind)) {
					changed--;
					continue;
				}

				// A place NEC never wrote names every DLL that changes it,
				// since NEC had no change there to lose. One NEC left alone
				// gets the again line, any other a line of its own. An owner
				// already named gets no line.
				if (place.size == 0) {
					if (AddOwner(place, OwnerAt(place.where, place.kind, true))) {
						(IsOff(place) ? again : late).push_back(&place);
					} else {
						changed--;
					}
					continue;
				}

				// A place NEC wrote, changed by another DLL. Follow the line.
				const auto walked = Walk(place);
				const auto reaches = walked.reaches == Reaches::kNec;

				// Cut below like any line that skips NEC, and worded once the
				// pass is over.
				if (walked.loops) {
					loops.push_back(LoopLine(place, Cutter(place, walked)));
				}

				// Stubs alone that lead straight to NEC's hook, as a DLL that
				// unhooks with a fresh stub of its own leaves the place. NEC's
				// hook runs on every call there, as if NEC had written it.
				if (reaches && walked.dlls.empty()) {
					returned();
					continue;
				}

				// Read before the walk's line goes in, see Said.
				const auto first = !Said(place);
				place.line = walked.dlls;

				// A new change ends any wait of the last one. A place every
				// open of its menu calls waits for that menu, see MenuWait.h,
				// when its new walk may not reach NEC, unless it is off
				// already or its hooks call each other without end. One whose
				// hook has not run yet waits again, whatever its new hook
				// reads.
				auto* menu = MenuWaitOf(place);
				if (menu) {
					Done(*menu);
				}
				auto* wait = menu && !IsOff(place) && !walked.loops && (!reaches || Proving(place)) ? menu : nullptr;

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
				if (!reaches && !wait) {
					AddOwner(place, Cutter(place, walked));
				}

				// Off for good already. Its first change reads as left to
				// another mod where the call still reaches NEC and the set need
				// not run on every call, else as a cut. Any later one reads as
				// changed again.
				if (place.share == Share::kCut) {
					if (first) {
						lines.push_back({ REX::ELogLevel::Warning,
							reaches && !everyCall ? ChangedLine(place, Tail(place)) : CutLine(place, everyCall, walked) });
					} else {
						again.push_back(&place);
					}
					continue;
				}

				if (everyCall || (!reaches && !wait)) {
					Lose(place, pass, Share::kCut);
					lines.push_back({ REX::ELogLevel::Warning, CutLine(place, everyCall, walked) });
				} else if (wait) {
					waits.push_back({ &place, wait, walked });
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

			Settle(waits, pass, lines, held);

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
				const auto off = PiecesOffTail(*place, true);
				// A place that comes back gets its part's "off for now" line
				// from Say too. A trace only place takes nothing off.
				if (!IsOff(*place) && !SwitchLeft(*place) && off.empty() && place->use != Use::kTraceOnly) {
					pass.parts.set(static_cast<std::size_t>(place->part));
				}
				// A held place is off only since a sibling's cut, as its own
				// walk reached NEC, and that cut's line comes first. A place
				// that waits for its menu is neither off nor left here.
				lines.push_back({ REX::ELogLevel::Warning,
					IsOff(*place) || SwitchLeft(*place) ? ChangedLine(*place, Tail(*place)) :
					!off.empty()                        ? ChangedLine(*place, off) :
					                                      ChangedLine(*place, WaitTail(*place), TraceTail(*place)) });
			}
			for (const auto* place : lone) {
				const auto off = PiecesOffTail(*place, true);
				// A place no mod takes anything from, see Part::kTrace and the
				// parts after it, loses nothing while the call still reaches
				// NEC, so its line is an INFO.
				const auto quiet = place->use == Use::kTraceOnly && off.empty();
				lines.push_back({ quiet ? REX::ELogLevel::Info : REX::ELogLevel::Warning,
					SwitchLeft(*place) ? ChangedLine(*place, Tail(*place)) :
					!off.empty()       ? ChangedLine(*place, off) :
					                     ChangedLine(*place, " NEC's change there keeps working as long as the call still reaches NEC."sv, TraceTail(*place)) });
			}
			// A set put to wait that has run since comes back on. Read
			// once the pass is over, so a set this pass made wait or stopped
			// never reads as back on, and none is said in a row whose switch
			// is left to another mod or for a place whose pieces are all off
			// anyway.
			for (auto& place : Places()) {
				auto* wait = WaitingOf(place);
				if ((HeldSet(place.set) || wait) && place.share == Share::kShared && RunOf(place.set) == Run::kLive) {
					place.share = Share::kProven;
					// Every DLL the walk found handed the call on, so the
					// Shared line names them all.
					if (wait) {
						place.line = Done(*wait);
					}
					if (!SwitchLeft(place) && PiecesOffTail(place, true).empty()) {
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
			for (auto& loop : loops) {
				lines.push_back({ REX::ELogLevel::Warning, std::move(loop) });
			}
			Say(pass, lines, false);
		}
		Write(lines);
		CountMenus();

		// A WARN only when a line of the pass is a warning or an error, or
		// turns a setting off.
		const auto warn = std::ranges::any_of(lines, [](const Line& a_line) {
			return !a_line.off.empty() || a_line.level == REX::ELogLevel::Warning || a_line.level == REX::ELogLevel::Error;
		});
		if (changed == 0) {
			REX::DEBUG("Checked {:d} places again as {:s}, none changed.", checked, a_moment);
		} else if (warn) {
			REX::WARN("Checked {:d} places again as {:s}, {:d} changed.", checked, a_moment, changed);
		} else {
			REX::INFO("Checked {:d} places again as {:s}, {:d} changed.", checked, a_moment, changed);
		}
		// A verdict can turn a switch off with no change of bytes, and the
		// Settings line still has to say so.
		return (changed != 0 && Summary() != before) || judged;
	}
}
