#include "Core/CallPatch/Ledger.h"

#include "Core/Feature.h"
#include "Core/Settings.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The ledger: every place NEC patches, noted while its row's Install runs and
// written once the turn ends, so a row is checked whole before anything of it
// is written. Recheck.cpp reads every noted place back later, written or not,
// since a DLL that patches after NEC can only be seen then.
//
// The lock is taken only by the functions CallPatch.h declares and by Note
// and SayPatched. The helpers run under the caller's lock and never take it,
// and log lines are gathered under the lock and written after it.
namespace CallPatch
{
	// Fills a hook's Link as its place is written, see LinkBase.
	struct Fill
	{
		static void Into(LinkBase& a_link, const Place& a_place)
		{
			a_link._next = a_place.next;
			a_link._set = a_place.set;
			a_link._mark = a_place.mark;
		}
	};

	namespace
	{
		std::mutex         g_lock;
		std::vector<Place> g_places;
		const Feature*     g_row = nullptr;  // whose turn it is
		std::size_t        g_turn = 0;       // the first place of the turn

		// Sets handed out so far, and the one an open Together fills.
		std::uint32_t g_sets = 0;
		std::uint32_t g_together = 0;
		Part          g_togetherPart = Part::kNone;
		std::uint32_t g_depth = 0;

		// Whether every place of a set is one NEC writes.
		std::array<bool, MAX_SETS> g_whole{};

		// Every switch left to another mod, and the part it went off with.
		std::vector<Yielded> g_yields;

		// The lines SayPatched holds back for End in the open turn.
		std::vector<std::string> g_patched;

		std::uint32_t NewSet()
		{
			if (g_sets + 1 >= MAX_SETS) {
				return 0;
			}
			g_sets++;
			g_whole[g_sets] = true;
			return g_sets;
		}

		// The part a place of the open turn belongs to: the open Together's,
		// then the one its call names, then the row's own.
		Part Resolved(Part a_part)
		{
			const auto part = g_together ? g_togetherPart : a_part;
			return part == Part::kNone ? g_row->part : part;
		}

		// What losing a place of a_part costs in a_row.
		Use UseOf(Part a_part, const Feature& a_row)
		{
			if (a_part == Part::kTrace) {
				return Use::kTraceOnly;
			}
			return a_part == a_row.part ? Use::kMain : Use::kDetail;
		}

		// Whether a_part is another row's: its own part, or a part a place of
		// it was noted with. A part belongs to 1 row, so its loss reads the
		// same everywhere.
		bool OfAnotherRow(Part a_part)
		{
			return std::ranges::any_of(Features(), [&](const Feature& a_row) { return &a_row != g_row && a_row.part == a_part; }) ||
			       std::ranges::any_of(g_places, [&](const Place& a_place) { return a_place.row != g_row && a_place.part == a_part; });
		}

		bool NoteLocked(const Ask& a_ask, Lines& a_lines)
		{
			if (!g_row) {
				a_lines.push_back({ REX::ELogLevel::Error,
					std::format("{:s} at {:X} is patched outside install, so it is left alone.", a_ask.what, a_ask.where) });
				return false;
			}

			// A call in a set names no part but the set's, so the set's loss
			// reads as 1 part.
			const auto part = Resolved(a_ask.part);
			if (g_together && a_ask.part != Part::kNone && a_ask.part != part) {
				a_lines.push_back({ REX::ELogLevel::Error,
					std::format("{:s} at {:X} names a different part than the rest of its set, so NEC leaves it alone.", a_ask.what, a_ask.where) });
				g_whole[g_together] = false;
				return false;
			}
			if (part == Part::kNone || (part != Part::kTrace && OfAnotherRow(part))) {
				a_lines.push_back({ REX::ELogLevel::Error,
					std::format("{:s} at {:X} has no part, or one of another row, so NEC leaves it alone.", a_ask.what, a_ask.where) });
				if (g_together) {
					g_whole[g_together] = false;
				}
				return false;
			}

			const auto set = g_together ? g_together : NewSet();
			const auto twice = std::ranges::any_of(g_places, [&](const Place& a_place) { return a_place.where == a_ask.where; });
			if (twice || set == 0 || g_places.size() >= MAX_PLACES) {
				a_lines.push_back({ REX::ELogLevel::Error,
					std::format("{:s} at {:X} is {:s}, so it is left alone.", a_ask.what, a_ask.where,
						twice ? "patched twice by NEC" : "one place too many for NEC's ledger") });
				g_whole[set] = false;
				return false;
			}

			Place place{
				.row = g_row,
				.set = set,
				.part = part,
				.use = UseOf(part, *g_row),
				.what = a_ask.what,
				.where = a_ask.where,
				.kind = a_ask.kind,
				.hook = a_ask.hook,
				.game = a_ask.game,
				.link = a_ask.link,
				.free = a_ask.free,
				.mark = static_cast<std::uint16_t>(g_places.size()),
			};
			// Every place is watched from now on, also one NEC leaves alone,
			// so a mod that writes over it later is named too.
			place.watch = SizeOf(place.kind);
			std::memcpy(place.seen.data(), reinterpret_cast<const void*>(place.where), place.watch);
			// The game's own is handed on to, or none for a call through a
			// vtable, which the hook makes itself, or a DLL's plain hook.
			if (place.free) {
				place.next = place.kind == Kind::kVirtualCall ? 0 : LeadsTo(place.where, place.kind);
			} else if (auto found = BuildableAt(a_ask); found.why == Why::kNone) {
				place.free = true;
				place.next = found.next;
				place.under = std::move(found.under);
			} else {
				place.why = found.why;
				place.owners.push_back(OwnerAt(place.where, place.kind, false));
				a_lines.push_back({ REX::ELogLevel::Warning, ClashLine(place) });
				g_whole[set] = false;
			}
			const auto free = place.free;
			g_places.push_back(std::move(place));
			return free;
		}

		void WritePlace(Place& a_place)
		{
			// The Link first, so the hook hands on from its very first call.
			if (a_place.link) {
				Fill::Into(*a_place.link, a_place);
			}
			auto& trampoline = REL::GetTrampoline();
			if (a_place.kind == Kind::kCall) {
				trampoline.write_call<REL32_SIZE>(a_place.where, a_place.hook);
				a_place.size = REL32_SIZE;
			} else if (a_place.kind == Kind::kJump) {
				trampoline.write_jmp<REL32_SIZE>(a_place.where, a_place.hook);
				a_place.size = REL32_SIZE;
			} else if (a_place.kind == Kind::kVirtualCall) {
				trampoline.write_call<VCALL_SIZE>(a_place.where, a_place.hook);
				a_place.size = VCALL_SIZE;
			} else {
				REL::WriteSafeData(a_place.where, a_place.hook);
				a_place.size = sizeof(std::uintptr_t);
			}
			std::memcpy(a_place.wrote.data(), reinterpret_cast<const void*>(a_place.where), a_place.size);
			a_place.seen = a_place.wrote;
			a_place.watch = a_place.size;
			if (!a_place.under.empty()) {
				a_place.share = Share::kOnTop;
			}
		}

		// Leaves a row's switch off while the game runs: off in memory only,
		// and every hook that asks it gives the game's own result. Once per
		// switch. Say writes the lines.
		void TurnOff(const Feature& a_row, Part a_cause, Pass& a_pass)
		{
			if (!a_row.on || YieldedLocked(a_row.on)) {
				return;
			}
			const auto was = a_row.on->GetValue();
			a_row.on->SetValue(false);
			g_yields.push_back({ a_row.on, a_cause });
			a_pass.offs.push_back({ &a_row, a_cause, was });
		}

		// End's rule. A taken place of the row's own part leaves a row with a
		// switch unwritten, since the switch is the player's whole of it.
		// Every taken place of the turn is lost, so each mod that has one is
		// named. Otherwise every set whose places are all free goes in, and a
		// set with a taken place is left whole.
		void EndLocked(Lines& a_lines)
		{
			const auto first = g_places.begin() + static_cast<std::ptrdiff_t>(g_turn);
			const std::span turn{ first, g_places.end() };
			Pass pass;

			if (g_row->on && std::ranges::any_of(turn, [](const Place& a_place) { return !a_place.free && a_place.use == Use::kMain; })) {
				g_patched.clear();
				for (auto& place : turn) {
					if (!place.free) {
						Lose(place, pass, Share::kLeft);
					}
				}
				Say(pass, a_lines, true);
				return;
			}

			for (auto& line : g_patched) {
				a_lines.push_back({ REX::ELogLevel::Info, std::move(line) });
			}
			g_patched.clear();

			for (auto& place : turn) {
				if (!g_whole[place.set]) {
					if (!place.free) {
						Lose(place, pass, Share::kLeft);
					}
					continue;
				}
				WritePlace(place);
			}

			// Flagged once every place of the turn is in, so no hook sees half
			// a set. SayOnTop writes the on top lines once every row is in.
			for (const auto& place : turn) {
				if (place.size != 0) {
					Start(place.set);
				}
			}
			Say(pass, a_lines, true);
		}
	}

	std::mutex& Lock()
	{
		return g_lock;
	}

	std::vector<Place>& Places()
	{
		return g_places;
	}

	const Yielded* YieldLocked(const Settings::Live<bool>* a_switch)
	{
		const auto it = std::ranges::find(g_yields, a_switch, &Yielded::on);
		return it != g_yields.end() ? &*it : nullptr;
	}

	bool YieldedLocked(const Settings::Live<bool>* a_switch)
	{
		return YieldLocked(a_switch) != nullptr;
	}

	// Its set's hooks give the game's own result from now on. A place of a
	// switched row's own part turns the switch off, and so does losing a
	// part another row rides on.
	void Lose(Place& a_place, Pass& a_pass, Share a_share)
	{
		Stop(a_place.set);
		a_place.share = a_share;
		if (a_place.use == Use::kTraceOnly) {
			return;
		}
		a_pass.parts.set(static_cast<std::size_t>(a_place.part));
		if (a_place.use == Use::kMain && a_place.row->on) {
			TurnOff(*a_place.row, a_place.part, a_pass);
		}
		for (const auto& row : Features()) {
			if (row.needs == a_place.part) {
				TurnOff(row, a_place.part, a_pass);
			}
		}
	}

	void Write(const Lines& a_lines)
	{
		for (const auto& line : a_lines) {
			if (!line.off.empty()) {
				Settings::LogChange(line.off, "true", "false", line.text);
			} else if (line.level == REX::ELogLevel::Error) {
				REX::ERROR("{:s}", line.text);
			} else if (line.level == REX::ELogLevel::Warning) {
				REX::WARN("{:s}", line.text);
			} else if (line.level == REX::ELogLevel::Debug) {
				REX::DEBUG("{:s}", line.text);
			} else {
				REX::INFO("{:s}", line.text);
			}
		}
	}

	bool Note(const Ask& a_ask)
	{
		Lines lines;
		bool  noted = false;
		{
			const std::scoped_lock l{ g_lock };
			noted = NoteLocked(a_ask, lines);
		}
		Write(lines);
		return noted;
	}

	void SayPatched(Part a_part, std::string a_line)
	{
		{
			const std::scoped_lock l{ g_lock };
			if (g_row && g_row->on && UseOf(Resolved(a_part), *g_row) == Use::kMain) {
				g_patched.push_back(std::move(a_line));
				return;
			}
		}
		REX::INFO("{:s}", a_line);
	}

	Together::Together(Part a_part, bool a_everyCall)
	{
		const std::scoped_lock l{ g_lock };
		if (g_depth++ == 0) {
			g_together = NewSet();
			g_togetherPart = a_part;
			if (g_together) {
				Mark(g_together, !a_everyCall, a_everyCall);
			}
		}
		set = g_together;
	}

	Together::~Together()
	{
		const std::scoped_lock l{ g_lock };
		if (--g_depth == 0) {
			g_together = 0;
			g_togetherPart = Part::kNone;
		}
	}

	Held Together::Set() const
	{
		const std::scoped_lock l{ g_lock };
		const auto noted = std::ranges::any_of(g_places, [&](const Place& a_place) { return a_place.set == set; });
		return set != 0 && noted && g_whole[set] ? Held{ set } : Held{};
	}

	void Begin(const Feature& a_row)
	{
		const std::scoped_lock l{ g_lock };
		g_row = &a_row;
		g_turn = g_places.size();
	}

	void End()
	{
		Lines lines;
		{
			const std::scoped_lock l{ g_lock };
			if (g_row) {
				EndLocked(lines);
				g_row = nullptr;
			}
		}
		Write(lines);
	}

	bool IsYielded(const Settings::Live<bool>& a_switch)
	{
		const std::scoped_lock l{ g_lock };
		return YieldedLocked(&a_switch);
	}
}
