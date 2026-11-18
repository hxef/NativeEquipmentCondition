#pragma once

#include "Core/Plugin.h"

#include "UI/Flash.h"
#include "UI/Hud/HudParts/Readout.h"

#include <optional>
#include <string>

// The CND by the ammo counter at Weapon CND Percent, a table of 2 lines beside
// the counter's 2 numbers. The readout's bar hides and its word stays where
// it stands at Bar, past the counts, its foot on the clip number's baseline.
// The percent stands under it in the word's letters, left aligned with it, its
// digits as far under the counter's line as the reserve number's are. A line
// box of NEC's own, a child of the readout, takes the line's thickness and runs
// it on from its right end to the end of the wider of the 2 lines, so 031 and
// CND stand over 1 line and 806 and 87.4% under it. Vanilla's line shows and
// keeps its own length. Off the line's place, moved by fHudBarX or fHudBarY,
// beside the counter, standing alone or by a melee weapon, the line box hides
// and the 2 lines keep their places to each other. Spot.h chooses their spot
// as for the bar, with the 2 lines as the word's box. Private to this folder.
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

	// The 2 lines as laid out for a word, a set of rows and the widest number
	// CND decimals writes. place is where both lines' letters stand from the
	// readout's origin with the word at its own place, the number as wide as
	// its widest, which is the box Spot.h moves and holds on screen. rise is
	// how far the word's field moves down from its place at Bar, number
	// where the number's field goes.
	struct Table
	{
		HudParts::Readout::LabelPlace place;
		double                        rise = 0.0;
		Flash::Point                  number;

		// What it was laid out for.
		Rows        rows;
		Flash::Box  word;
		std::string widest;
	};

	// The table for a_word, the word's place as Spot.h's Label measured it,
	// and a_rows, or a_last when nothing it was laid out for changed. Writes
	// the widest numbers into the number field to measure them, so the
	// number is written again after a new table. Nothing when the movie
	// refuses the number field or the word is not measured yet.
	[[nodiscard]] std::optional<Table> LayTable(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_readout, const HudParts::Readout::LabelPlace& a_word, const Rows& a_rows,
		const std::optional<Table>& a_last);

	// Moves the word's and the number's fields for a_table, moved by a_x and
	// a_y as Spot.h moves the word, shows the number's field and hides the
	// line box until DrawLine shows it. a_word is the word's place at Bar.
	void PutTable(Scaleform::GFx::Value& a_readout, const HudParts::Readout::LabelPlace& a_word, const Table& a_table, double a_x, double a_y);

	// Writes a_number into the number's field in the word's letters.
	void WriteNumber(Scaleform::GFx::Value& a_readout, const std::string& a_number);

	// Shows the line box while a_lined, from the line's right end to the
	// end of the wider of the 2 lines as they read now, else hides it.
	void DrawLine(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_readout, const Table& a_table, bool a_lined);

	// Fades the line box to a_alpha of the readout's own, so it fades with
	// vanilla's line while the readout stays.
	void FadeLine(Scaleform::GFx::Value& a_readout, double a_alpha);

	// Hides the number's field and the line box, for a change back to Bar.
	void HideTable(Scaleform::GFx::Value& a_readout);
}
