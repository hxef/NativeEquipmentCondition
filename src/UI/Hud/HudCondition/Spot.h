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

	// Where the bar sits. line.x and line.y are the right end of the bar and
	// its middle, line.width its length. wordX and wordY are how far the
	// word moved from its place past the bar's end.
	struct Spot
	{
		Roles::Hud::Line line;
		double           deep = 0.0;
		bool             onDivider = true;
		double           wordX = 0.0;
		double           wordY = 0.0;

		bool operator==(const Spot&) const = default;
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

	// Writes the word with a_first, once a movie since it never changes,
	// and returns where it sits from the bar's right end, a_moved taken
	// back, the wordX and wordY the word was last moved by. Both have their
	// origin on their right end, so the bar runs left and the word follows
	// past it. The trace says the first try and the one that measures.
	[[nodiscard]] HudParts::Readout::LabelPlace Label(Roles::Value& a_readout, bool a_first, const Flash::Point& a_moved);

	// Measures the HUD around the counter now and chooses the spot. Nothing
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
	// nearest free spot beside the bar, past its end first.
	//
	// Otherwise the bar and the word past its end take the nearest free spot
	// above, below, left or right of what the counter draws, a bar's depth
	// away, keeping off the divider, which shows again. A spot where only
	// the word's letters fit is taken when none fits its whole line. With
	// none free the bar keeps the divider's place, as before NEC looked for
	// a spot, its word placed as on a divider that fits, and NEC.log says
	// so. Widgets a mod adds in another layer of the movie are not avoided.
	[[nodiscard]] std::optional<Choice> Choose(Scaleform::GFx::Movie& a_movie, Roles::Value& a_counter, const Roles::Value& a_readout,
		std::optional<bool> a_ownVisible, const HudParts::Readout::LabelPlace& a_label);

	// The box the bar and its word fill from the bar's right end, which the
	// HUD holds whole on screen. It reaches down to the bottom of the word's
	// line, since letters in some languages, Chinese for one, reach below
	// the baseline.
	[[nodiscard]] Flash::Box Filled(const Spot& a_spot, const HudParts::Readout::LabelPlace& a_label);

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
