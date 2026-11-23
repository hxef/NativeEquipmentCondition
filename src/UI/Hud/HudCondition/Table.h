#pragma once

#include "Core/Plugin.h"

#include "UI/Flash.h"
#include "UI/Hud/HudParts/Readout.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// The CND by the ammo counter at Weapon CND Percent, a table of 2 lines beside
// the counter's 2 numbers. The readout's bar hides and its word stays where
// it stands at Bar, past the counts, its foot on the clip number's baseline.
// The percent stands under it in the word's letters, left aligned with it, its
// digits as far under the counter's line as the reserve number's are. A line
// box of NEC's own, a child of the readout, takes the line's thickness and runs
// it on from its right end to the end of the wider of the 2 lines, so 031 and
// CND stand over 1 line and 806 and 87.4% under it. Vanilla's line shows and
// keeps its own length. Spot.h chooses their spot as for the bar, with the 2
// lines as the word's box.
//
// The 2 lines stand only where the line runs between them, see Lines.
// Anywhere else the percent reads in 1 line, CND 87.4%, in the word's own
// field at the word's place, its left edge and its baseline. With
// bHudBarPercentOneLine on it reads so at the line too, drawn smaller where it
// would run off screen, and the line box runs the line on under the whole of
// it. Private to this folder.
namespace HudCondition
{
	// Where the counter's line and its 2 numbers stand, from the readout's
	// origin at the line's place, in the counter's units: the line's top,
	// bottom and right end, the clip number's baseline over it and where the
	// reserve number's digits start under it.
	struct Rows
	{
		double top = -1.0;
		double bottom = 1.0;
		double end = 0.0;
		double foot = -7.7;
		double head = 7.7;

		bool operator==(const Rows&) const = default;
	};

	// The rows as the movie shows them now. A part that cannot be read keeps
	// what a_last holds, which starts as the shipped counter: a line 2 thick
	// and the 2 numbers 6.7 from it.
	[[nodiscard]] Rows RowsNow(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_counter, std::optional<bool> a_ownVisible, const Rows& a_last);

	// The 1 line, CND 87.4%, as laid out at its widest: where its field and
	// letters stand from the readout's origin, the corner its field goes to,
	// and its size as a share of the word's.
	struct OneLine
	{
		HudParts::Readout::LabelPlace place;
		Flash::Point                  corner;
		double                        scale = 1.0;
	};

	// The 2 lines as laid out for a word, a set of rows and the widest number
	// CND decimals writes. place is where both lines' letters stand from the
	// readout's origin with the word at its own place, the number as wide as
	// its widest, which is the box Spot.h moves and holds on screen. away is
	// the 1 line at the word's place in the word's letters, and atLine the
	// same at the ammo line, drawn smaller where it would run off screen,
	// see LayTable. rise is how far the word's field moves down from its
	// place at Bar, number where the number's field goes.
	struct Table
	{
		HudParts::Readout::LabelPlace place;
		OneLine                       away;
		OneLine                       atLine;
		double                        rise = 0.0;
		Flash::Point                  number;

		// What it was laid out for.
		Rows        rows;
		Flash::Box  word;
		std::string widest;
	};

	// The table for a_word, the word's place as Spot.h's Label measured it,
	// and a_rows, or a_last when nothing it was laid out for changed. Writes
	// the widest lines and numbers into the number field to measure them, so
	// the number is written again after a new table. Nothing when the movie
	// refuses the number field or the word is not measured yet.
	//
	// a_lineRight is the screen's right edge from the readout's origin, see
	// Choice in Spot.h. Right of the clip number the 1 line at the ammo line
	// runs past it on a 16:9 screen, in vanilla and HUD mods alike, so it
	// is drawn smaller there until it ends on screen, never under 0.6 of the
	// word's size. Its left edge and baseline stay the word's.
	[[nodiscard]] std::optional<Table> LayTable(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_readout, const HudParts::Readout::LabelPlace& a_word, const Rows& a_rows,
		const std::optional<Table>& a_last, std::optional<double> a_lineRight);

	// What the percent's form is told from, each a plain fact of the place
	// and the weapon. None follows the counter's fade, so the form holds
	// while the same gun is held at the same place.
	struct LinesFacts
	{
		bool moved = false;      // fHudBarX or fHudBarY is not 0
		bool alone = false;      // it stands alone, with no counter on screen
		bool noCounter = false;  // the weapon takes no ammo, so the game never shows the counter
		bool between = false;    // the divider runs between the 2 numbers, see Choice in Spot.h
		bool fits = false;       // the 2 lines fit by it, the same
		bool asked = false;      // bHudBarPercentOneLine is on
	};

	// The percent's form, and why in the words NEC.log says it with. atLine
	// is true while it stands at the divider, which the line box may run on.
	struct Lines
	{
		bool             atLine = false;
		bool             one = false;
		std::string_view why;
	};

	// The rule. The percent stands at the divider at its own place, by a
	// counter that shows this weapon's rounds and runs its divider between
	// its 2 numbers. There it reads in 2 lines where they fit, else in 1, and
	// in 1 with bHudBarPercentOneLine on. Anywhere else it reads in 1 line.
	[[nodiscard]] Lines LinesFor(const LinesFacts& a_facts);

	// Moves the word's and the number's fields for a_table, moved by a_x and
	// a_y as Spot.h moves the word, shows the number's field, or hides it
	// and puts the word's field at a_one's corner for the 1 line, and hides
	// the line box until DrawLine shows it. a_word is the word's place at Bar.
	void PutTable(Scaleform::GFx::Value& a_readout, const HudParts::Readout::LabelPlace& a_word, const Table& a_table, double a_x, double a_y, const OneLine* a_one);

	// Writes a_number into the number's field in the word's letters.
	void WriteNumber(Scaleform::GFx::Value& a_readout, const std::string& a_number);

	// Writes the 1 line for a_tenths into the word's own field, CND 87.4%,
	// a_scale times the word's size.
	void WriteOneLine(Scaleform::GFx::Value& a_readout, std::uint32_t a_tenths, double a_scale);

	// Writes the word alone back into its own field, as the 1 line goes.
	void WriteWord(Scaleform::GFx::Value& a_readout);

	// Shows the line box while a_lined, from the line's right end to the
	// end of the wider of the 2 lines as they read now, or of the 1 line
	// for a_oneLine, else hides it.
	void DrawLine(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_readout, const Table& a_table, bool a_lined, bool a_oneLine);

	// Fades the line box to a_alpha of the readout's own, so it fades with
	// vanilla's line while the readout stays.
	void FadeLine(Scaleform::GFx::Value& a_readout, double a_alpha);

	// Hides the number's field and the line box, for a change back to Bar.
	void HideTable(Scaleform::GFx::Value& a_readout);
}
