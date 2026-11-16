#pragma once

#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Roles/Hud.h"

#include <chrono>
#include <optional>
#include <string>

// Where the CND bar and its word go by the ammo counter: what the HUD shows
// around the counter, measured through Roles::Hud in the counter's units, and
// the spot that leaves for them. Private to this folder.
namespace HudCondition
{
	// The bar is 3 times as thick as the divider it stands for, 6 deep on
	// vanilla's divider, which is 2 thick.
	inline constexpr double DEEP_PER_THICK = 3.0;

	// A Spot as a measure gives it the other way round, all but its depth,
	// which a flip never changes.
	struct Turn
	{
		Roles::Hud::Line line;
		bool             onDivider = true;
		double           wordX = 0.0;
		double           wordY = 0.0;
		double           clear = 0.0;

		bool operator==(const Turn&) const = default;
	};

	// Where the bar sits. line.x and line.y are the right end of the bar and
	// its middle, line.width its length. wordX and wordY are how far the
	// word moved from its place past the bar's end. A flipped bar fills from
	// its left end and its word's own place is before the bar's start, the
	// mirror of the place past the end, or further left to keep its gap from
	// the counter's numbers, so its wordX is never 0. clear is how far
	// further left that is, worked out either way round. turn is the spot
	// the same measure gives the other way round, empty for a spot no
	// measure made. Turned reads the 2, so a flip needs no new measure.
	struct Spot
	{
		Roles::Hud::Line    line;
		double              deep = 0.0;
		bool                onDivider = true;
		double              wordX = 0.0;
		double              wordY = 0.0;
		bool                flipped = false;
		double              clear = 0.0;
		std::optional<Turn> turn = std::nullopt;

		bool operator==(const Spot&) const = default;

		// True when a_other sits as this spot does, the same or different
		// by clear and turn alone, so the bar and its word do not move.
		// The 2 change as a number is shown or hidden, and only Turned
		// reads them.
		[[nodiscard]] bool SitsAs(const Spot& a_other) const
		{
			auto same = a_other;
			same.clear = clear;
			same.turn = turn;
			return same == *this;
		}
	};

	// A spot and the words NEC.log says it with, empty for the bar on the
	// divider with its word past its end. key is the same without its
	// numbers, so NEC.log says a spot and a kind of reason once however the
	// measured numbers change.
	struct Choice
	{
		Spot        spot;
		std::string said;
		std::string key;
	};

	// How far the word moves from its place past the end of a bar a_length
	// long to its own place: 0, or while a_flipped the mirror of a_letters
	// about the bar's middle, before the bar's start.
	[[nodiscard]] double Home(bool a_flipped, double a_length, const Roles::Box& a_letters);

	// a_spot the way round a_flipped says, for a flip while Choose answers
	// nothing. A spot a measure made takes its turn, the spot that measure
	// gives the other way round. Otherwise it stays where it sits, a word
	// in the bar's row on its own side goes to its own place on the other
	// side, see Home and the spot's clear, and any other word stays where
	// it is. a_letters are the word's, as for Home.
	[[nodiscard]] Spot Turned(const Spot& a_spot, bool a_flipped, const Roles::Box& a_letters);

	// Writes the word with a_first, once a movie since it never changes,
	// and returns where it sits from the bar's right end, a_moved taken
	// back, the wordX and wordY the word was last moved by. Both have their
	// origin on their right end, so the bar runs left and the word follows
	// past it, which is the place a flipped spot mirrors. The trace says the
	// first try and the one that measures.
	[[nodiscard]] HudParts::Readout::LabelPlace Label(Roles::Value& a_readout, bool a_first, const Flash::Point& a_moved);

	// Measures the HUD around the counter now and chooses the spot, and the
	// one the other way round as its turn. Nothing
	// while the divider is hidden or off screen, so the bar keeps its last
	// spot. a_ownVisible is the divider's own visible while NEC covers it,
	// and a_label where the word sits from the bar's right end.
	//
	// The bar and the word never cover a shown number or another part, text
	// counted across its field and down to its letters' end, and stay on
	// screen while the counter is, the word down to the bottom of its line
	// as Filled holds it. A child of the counter whose box holds all its
	// numbers is a backdrop they sit on, as the numbers do.
	//
	// The divider's own place comes first. The bar fits there when the
	// divider is upright and at least 4 times as long as the bar is deep.
	// The numbers count as clear when the divider runs between them, 1
	// above and 1 below, as the game draws them. The word then takes the
	// nearest free spot beside the bar, its own place first: past the bar's
	// end, or before its start while a_flipped, as far left as it takes to
	// keep its gap from the bar from each number right of it too. A number
	// that asks more than twice that gap covers the word there.
	//
	// Otherwise the bar and the word at its own place take the nearest free spot
	// above, below, left or right of what the counter draws, a bar's depth
	// away, keeping off the divider, which shows again. A spot where only
	// the word's letters fit is taken when none fits its whole line. With
	// none free the bar keeps the divider's place, as before NEC looked for
	// a spot, its word placed as on a divider that fits, and NEC.log says
	// so. Widgets a mod adds in another layer of the movie are not avoided.
	[[nodiscard]] std::optional<Choice> Choose(Scaleform::GFx::Movie& a_movie, Roles::Value& a_counter, const Roles::Value& a_readout,
		std::optional<bool> a_ownVisible, const HudParts::Readout::LabelPlace& a_label, bool a_flipped);

	// The box the bar and its word fill from the bar's right end, which the
	// HUD holds whole on screen. It reaches down to the bottom of the word's
	// line, since letters in some languages, Chinese for one, reach below
	// the baseline.
	[[nodiscard]] Flash::Box Filled(const Spot& a_spot, const HudParts::Readout::LabelPlace& a_label);

	// a_spot with its bar a_size times its own length and thickness, about
	// the bar's right end and middle, so the spot and the shift keep their
	// meaning. Choose works at the bar's own size, so a size never moves the
	// spot. A word past the bar's end stays where it is, and a word moved
	// above, below or left of the bar keeps its gap from that side. A
	// flipped bar keeps its left end instead and grows right. A word right
	// of it then moves with its right end, and any other word stays where
	// it is across.
	[[nodiscard]] Spot Sized(const Spot& a_spot, const HudParts::Readout::Size& a_size, const HudParts::Readout::LabelPlace& a_label);

	// Says a choice in NEC.log once it has held for a second, so a HUD mod
	// still laying its parts out after a load writes nothing. Each key is
	// said once a game start, since every save load builds the HUD again.
	// Used on the HUD's thread alone.
	class Teller
	{
	public:
		using Clock = std::chrono::steady_clock;

		// Called with every reading's choice.
		void Tell(const Choice& a_choice, Clock::time_point a_now);

		// Forgets the choice held, for a new HUD movie.
		void Reset();

	private:
		std::string       held;
		Clock::time_point since;
	};
}
