#pragma once

#include "Core/Plugin.h"

#include "UI/Inventory/PaperDoll/Regions.h"
#include "UI/Inventory/PaperDoll/Room.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

// The apparel figure at Apparel figure CND Percent: CND and the lowest percent
// over each body part, "CND 30.0%", in bold letters in the word field of the
// bar's readout, whose bar hides. The bar keeps being laid out, and each
// number stands by it with its baseline where the bar's bottom is, 2 above
// the icon. The head's, right arm's and right leg's numbers are centred on
// their bars, the left arm's and left leg's start where their bars start, and
// the torso's ends where its bar ends while its entry shows 1 digit, clear of
// the left leg's entry. All 6 take 1 size, the largest the tightest room
// above them allows, from 0.75 of the figure's size 18 down to 0.5 of it, see
// Room.h. Private to this folder.
namespace PaperDoll
{
	// The size the numbers start at and the least they shrink to.
	inline constexpr double NUMBER_SIZE = 0.75 * 18.0;
	inline constexpr double NUMBER_LEAST = 0.5 * 18.0;

	// What a region's number stands by: its bar as laid out this frame, and
	// how long that bar is while its entry shows 1 digit, see ShortWidth.
	struct NumberSpot
	{
		Placement placed;
		double    shortWidth{ 0.0 };

		bool operator==(const NumberSpot&) const = default;
	};

	// A region's entry on the figure, the readout over it and its spot.
	struct NumberEntry
	{
		Value      entry;
		Value      readout;
		NumberSpot spot;
	};

	using NumberEntries = std::array<std::optional<NumberEntry>, kRegions>;

	// The numbers of 1 Pip-Boy movie, kept between frames so only a change
	// is written.
	class Numbers
	{
	public:
		// Writes and places the numbers once a number, a spot or the room
		// above may have changed. a_entries holds each region whose entry
		// and readout were found this frame, a_texts the header's
		// texts as HeaderTexts gave them. a_relaid says a bar was laid out
		// again this frame, as a header built after the figure lays it out.
		void Draw(Scaleform::GFx::Movie& a_movie, Value& a_doll, std::vector<Value>& a_texts, NumberEntries& a_entries, bool a_relaid);

		// Forgets what was drawn, so the next Draw writes everything again.
		void Forget();

	private:
		// What 1 region shows: its number with no word, and the spot it was
		// placed by.
		struct Shown
		{
			std::string               number;
			std::optional<NumberSpot> spot;
		};

		std::array<Shown, kRegions> shown;

		// The CND word as the movie translates it, read once a movie.
		std::string word;

		// The least headroom the trace last named as too little, see Draw.
		std::optional<double> tightSaid;
	};
}
