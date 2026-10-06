#include "Core/CallPatch/Ledger.h"

#include "Core/Feature.h"
#include "Core/Pieces.h"
#include "Core/Text/Text.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <iterator>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

// Which pieces of each part are off, worked out from the places and the
// switches turned off whenever asked, so there is nothing to keep in step, and
// the lines NEC.log says it in as it happens. A piece is off when a place of
// it is off, or when a piece it needs is off, see Core/Pieces.h.
//
// The helpers run under the caller's lock, and the functions CallPatch.h
// declares take it once.
namespace CallPatch
{
	namespace
	{
		// What a piece's places and needs leave of it. cause is the part whose
		// places were taken, root the piece of it, kNone where only a switch
		// says so.
		struct State
		{
			bool               off = false;
			bool               own = false;     // a place of its own is off
			bool               folded = false;  // off only as the switch it rides on is left to another mod
			Part               cause = Part::kNone;
			Piece              root = Piece::kNone;
			std::vector<Owner> owners;
		};

		using States = std::array<State, PIECE_COUNT>;

		State& At(States& a_states, Piece a_piece)
		{
			return a_states[static_cast<std::size_t>(a_piece)];
		}

		// The row whose own part a_part is, or nullptr for a part a patch call
		// names.
		const Feature* RowOf(Part a_part)
		{
			for (const auto& row : Features()) {
				if (row.part == a_part) {
					return &row;
				}
			}
			return nullptr;
		}

		// The row a_part's places were noted for.
		const Feature* RowOfPlaces(Part a_part)
		{
			const auto& places = Places();
			const auto  it = std::ranges::find(places, a_part, &Place::part);
			return it != places.end() ? it->row : nullptr;
		}

		// Whether a place counts as off for the page and NEC.log: off for good,
		// or in a held set that waits, whose hooks give the game's result until
		// the changed place's hook runs.
		bool OffNow(const Place& a_place)
		{
			return IsOff(a_place) || RunOf(a_place.set) == Run::kWaiting;
		}

		// Adds who has a place that counts as off to a_owners, each once: its
		// owners, or for a set that waits, the DLLs over each changed place of
		// that set whose hook has not run since.
		void AddOwners(std::vector<Owner>& a_owners, const Place& a_place)
		{
			const auto add = [&](const std::vector<Owner>& a_from) {
				for (const auto& owner : a_from) {
					if (std::ranges::find(a_owners, owner) == a_owners.end()) {
						a_owners.push_back(owner);
					}
				}
			};
			if (IsOff(a_place)) {
				add(a_place.owners);
				return;
			}
			for (const auto& place : Places()) {
				if (place.set == a_place.set && place.share == Share::kShared && Proving(place)) {
					add(place.line);
				}
			}
		}

		// The owners of every place of the part that is off, each once, in the
		// order of Before.
		std::vector<Owner> OwnersLocked(Part a_part)
		{
			std::vector<Owner> owners;
			for (const auto& place : Places()) {
				if (place.part == a_part && OffNow(place) && place.use != Use::kTraceOnly) {
					AddOwners(owners, place);
				}
			}
			std::ranges::sort(owners, Before);
			return owners;
		}

		// Every piece's state, with the places of set a_skip counted as on,
		// and the pieces of a_alone off only through what they need. Its own
		// places first, each marking every piece it is, then what it needs,
		// from the top, since a piece needs only pieces above it.
		States StatesLocked(std::uint32_t a_skip = 0, std::span<const Piece> a_alone = {})
		{
			States states{};
			for (const auto& place : Places()) {
				if (place.use == Use::kTraceOnly || place.set == a_skip || !OffNow(place)) {
					continue;
				}
				for (const auto piece : PiecesAt(place.what)) {
					if (std::ranges::find(a_alone, piece) != a_alone.end()) {
						continue;
					}
					auto& state = At(states, piece);
					state.off = state.own = true;
					state.cause = RowOfPiece(piece).part;
					state.root = piece;
					AddOwners(state.owners, place);
					std::ranges::sort(state.owners, Before);
				}
			}
			for (const auto& row : PieceRows()) {
				auto& state = At(states, row.piece);
				for (const auto need : row.needs) {
					if (state.off || need == Piece::kNone || !At(states, need).off) {
						continue;
					}
					const auto& from = At(states, need);
					state.off = true;
					state.cause = from.cause;
					state.root = from.root;
					state.owners = from.owners;
				}
				// A switch's own part is off with its switch, named after the
				// part it went off with.
				const auto* own = row.part != Part::kNone ? RowOf(row.part) : nullptr;
				const auto* yielded = !state.off && own && own->on ? YieldLocked(own->on) : nullptr;
				if (yielded) {
					state.off = true;
					state.cause = yielded->cause;
					state.owners = OwnersLocked(yielded->cause);
				}
			}
			// A detail of a switch left to another mod does nothing either way,
			// and the switch's line says so.
			for (auto* on : Settings::Switches()) {
				const auto* link = YieldedLocked(on) ? LinkOf(*on) : nullptr;
				if (!link) {
					continue;
				}
				for (const auto list : { link->side, link->exceptions }) {
					for (const auto piece : list) {
						auto& state = At(states, piece);
						state.folded = state.off && !state.own;
					}
				}
			}
			return states;
		}

		// Whether the player has on every switch a piece works through: the
		// switch of its part's row, and those of the pieces it needs, all the
		// way down. Fire rate shown in menus works through bFireRate that way.
		bool SwitchedOn(Piece a_piece)
		{
			const auto& row = RowOfPiece(a_piece);
			if (row.part != Part::kNone) {
				const auto* own = RowOf(row.part);
				const auto* feature = own ? own : RowOfPlaces(row.part);
				if (feature && feature->on && !feature->on->GetValue()) {
					return false;
				}
			}
			return std::ranges::all_of(row.needs, [](Piece a_need) { return a_need == Piece::kNone || SwitchedOn(a_need); });
		}

		// a_part's shown pieces that are off, grouped by cause and owners, and
		// the ones still working, which leaves out a piece whose switch the
		// player has off. A folded piece is neither.
		std::optional<Loss> LossFrom(Part a_part, const States& a_states)
		{
			Loss loss;
			loss.part = a_part;
			for (const auto& row : PieceRows()) {
				if (!ShownIn(row.piece, a_part)) {
					continue;
				}
				const auto& state = a_states[static_cast<std::size_t>(row.piece)];
				if (!state.off) {
					if (SwitchedOn(row.piece)) {
						loss.works.push_back(row.piece);
					}
					continue;
				}
				if (state.folded) {
					continue;
				}
				auto it = std::ranges::find_if(loss.causes,
					[&](const Cause& a_cause) { return a_cause.part == state.cause && a_cause.owners == state.owners; });
				if (it == loss.causes.end()) {
					loss.causes.push_back(Cause{ state.cause, {}, {}, state.owners });
					it = loss.causes.end() - 1;
				}
				it->pieces.push_back(row.piece);
				if (ShownIn(state.root, state.cause) && std::ranges::find(it->taken, state.root) == it->taken.end()) {
					it->taken.push_back(state.root);
				}
			}
			return loss.causes.empty() ? std::nullopt : std::optional{ std::move(loss) };
		}

		// The first sentence of a switch's line, the same in Say and OffLine:
		// "Slower fire when worn: off, left to A.dll.", or for a switch that
		// went off with another part "Jamming: off, since Gun wear from firing
		// is left to A.dll."
		std::string SwitchLine(Part a_part, Part a_cause, std::span<const Owner> a_owners)
		{
			const auto name = Text::PartLogName(a_part);
			if (OnlyVersion(a_owners)) {
				return std::format("{:s}: off, game version {:s} is not supported.", name, GameVersion());
			}
			if (a_cause != a_part) {
				return std::format("{:s}: off, since {:s} is left to {:s}.", name, Text::PartLogName(a_cause), Words(a_owners));
			}
			return std::format("{:s}: off, left to {:s}.", name, Words(a_owners));
		}

		// A switch a pass turned off, and the change of its setting when it
		// was on.
		void SayOff(const Pass::Off& a_off, Lines& a_lines, bool a_install)
		{
			const auto owners = OwnersLocked(a_off.cause);
			const auto key = a_off.row->on->key;
			// At install the switch's row patches nothing: its turn just
			// ended unwritten, or main.cpp skips it as it reaches it.
			const auto tail = a_install ? std::format(" NEC patches none of it, and {:s} stays false.", key) :
			                              std::format(" {:s} stays false.", key);
			a_lines.push_back({ REX::ELogLevel::Warning, SwitchLine(a_off.row->part, a_off.cause, owners) + tail });
			if (a_off.was) {
				a_lines.push_back({ REX::ELogLevel::Info,
					OnlyVersion(owners) ? std::format("as game version {:s} is not supported", GameVersion()) :
										  std::format("as {:s} is left to {:s}", Text::PartLogName(a_off.cause), Words(owners)),
					key });
			}
		}

		// A part with pieces off whose switch the pass did not turn off: 1
		// line a cause, every piece named.
		void SayPart(Part a_part, const States& a_states, Lines& a_lines)
		{
			const auto loss = LossFrom(a_part, a_states);
			if (!loss) {
				return;
			}
			auto said = Text::PartLines(*loss, Text::Out::kLog);
			// A whole smaller part of a row whose switch stands and is on.
			const auto* row = RowOfPlaces(a_part);
			if (loss->works.empty() && loss->causes.size() == 1 && loss->causes.front().part == a_part && row && row->on &&
				row->part != a_part && !YieldedLocked(row->on) && row->on->GetValue() && !said.empty()) {
				said.back() += std::format(" {:s} still works without it.", Text::PartLogName(row->part));
			}
			for (auto& line : said) {
				a_lines.push_back({ REX::ELogLevel::Warning, std::move(line) });
			}
		}

		// The switch a setting is, or the one it sits under, nullptr for a
		// number with none, such as one under a number.
		const Settings::Live<bool>* SwitchOf(const SettingLink& a_link)
		{
			const auto* lead = a_link.under ? a_link.under : a_link.setting;
			const auto  switches = Settings::Switches();
			const auto  it = std::ranges::find(switches, lead, [](const auto* a_on) { return static_cast<const Settings::Named*>(a_on); });
			return it != switches.end() ? *it : nullptr;
		}

		// Whether a piece of a_part is off for a part the pass lost a place of,
		// so its line is due too.
		bool OffWith(Part a_part, const Pass& a_pass, const States& a_states)
		{
			return std::ranges::any_of(PieceRows(), [&](const PieceRow& a_row) {
				const auto& state = a_states[static_cast<std::size_t>(a_row.piece)];
				return ShownIn(a_row.piece, a_part) && state.off && !state.folded && state.cause != a_part &&
				       a_pass.parts.test(static_cast<std::size_t>(state.cause));
			});
		}
	}

	std::string PiecesOffTail(const Place& a_place)
	{
		// Another place of a piece marked alone takes nothing from this one.
		const auto         pieces = PiecesAt(a_place.what);
		std::vector<Piece> alone;
		std::ranges::copy_if(pieces, std::back_inserter(alone), [](Piece a_piece) { return RowOfPiece(a_piece).alone; });
		auto states = StatesLocked(a_place.set, alone);
		if (pieces.empty() || !std::ranges::all_of(pieces, [&](Piece a_piece) { return At(states, a_piece).off; })) {
			return {};
		}
		const auto& state = At(states, pieces.front());
		const auto  root = ShownIn(state.root, state.cause) ? std::span{ &state.root, 1 } : std::span<const Piece>{};
		const auto  part = Text::PartPieces(state.cause, root, Text::Out::kLog);
		return OnlyVersion(state.owners) ? std::format(" NEC's change there does nothing while {:s} is off on game version {:s}, which is not supported.", part, GameVersion()) :
		                                   std::format(" NEC's change there does nothing while {:s} is left to {:s}.", part, Words(state.owners));
	}

	// A part's line, or a switch's line for a part that turned its switch
	// off, in the enum's order, and the line of a part a piece of which needs
	// what the pass lost. Then every switch that went off with a part it
	// rides on.
	void Say(const Pass& a_pass, Lines& a_lines, bool a_install)
	{
		const auto states = StatesLocked();
		for (std::size_t i = 0; i < PART_COUNT; i++) {
			const auto part = static_cast<Part>(i);
			const auto off = std::ranges::find_if(a_pass.offs, [&](const Pass::Off& a_off) { return a_off.row->part == part; });
			if (a_pass.parts.test(i) && off != a_pass.offs.end() && off->cause == part) {
				SayOff(*off, a_lines, a_install);
			} else if (a_pass.parts.test(i) || (off == a_pass.offs.end() && OffWith(part, a_pass, states))) {
				SayPart(part, states, a_lines);
			}
		}
		for (const auto& off : a_pass.offs) {
			if (off.cause != off.row->part) {
				SayOff(off, a_lines, a_install);
			}
		}
	}

	std::optional<Loss> LossOf(Part a_part)
	{
		const std::scoped_lock l{ Lock() };
		return LossFrom(a_part, StatesLocked());
	}

	std::vector<Loss> Losses()
	{
		const std::scoped_lock l{ Lock() };
		const auto             states = StatesLocked();
		std::vector<Loss>      losses;
		for (auto i = static_cast<std::size_t>(Part::kPerks); i < static_cast<std::size_t>(Part::kTrace); i++) {
			if (auto loss = LossFrom(static_cast<Part>(i), states)) {
				losses.push_back(std::move(*loss));
			}
		}
		return losses;
	}

	std::optional<Loss> YieldOf(const Settings::Live<bool>& a_switch)
	{
		const auto             part = PartOf(a_switch);
		const std::scoped_lock l{ Lock() };
		return YieldedLocked(&a_switch) ? LossFrom(part, StatesLocked()) : std::nullopt;
	}

	Effect EffectOf(const Settings::Named& a_setting)
	{
		const auto* link = LinkOf(a_setting);
		if (!link) {
			return {};
		}
		const std::scoped_lock l{ Lock() };
		const auto             states = StatesLocked();
		Effect                 effect;
		// Idle while the player has its switch off. A switch left to another
		// mod is off too, but its own line says why, so that is never idle.
		const auto* on = SwitchOf(*link);
		effect.idle = on && !on->GetValue() && !YieldedLocked(on);
		for (const auto piece : link->core) {
			(states[static_cast<std::size_t>(piece)].off ? effect.off : effect.works).push_back(piece);
		}
		for (const auto piece : link->side) {
			if (states[static_cast<std::size_t>(piece)].off) {
				effect.off.push_back(piece);
			}
		}
		if (link->always != Piece::kNone) {
			effect.works.push_back(link->always);
		}
		return effect;
	}

	std::string OffLine(const Settings::Live<bool>& a_switch)
	{
		const auto             part = PartOf(a_switch);
		const std::scoped_lock l{ Lock() };
		const auto*            yielded = YieldLocked(&a_switch);
		return yielded ? SwitchLine(part, yielded->cause, OwnersLocked(yielded->cause)) : std::string{};
	}
}
