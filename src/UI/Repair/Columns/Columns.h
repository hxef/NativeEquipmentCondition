#pragma once

#include "Core/Plugin.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The rows of the 3 repair boxes in columns: the spare box and the repair level
// boxes of the bench and of a trader. The box sets each row as html, so a row
// that carries its own tab stops lines its cells up under the other rows'. Each
// row is a centred paragraph as wide as the block, so the block is centred in
// whatever field draws the rows, every row starting at the same left edge.
// Nothing in the box's movie is written. Each column is as wide as its widest
// cell, measured in the font the box draws its rows in, with a gap after it.
// With bRepairBoxColumns off, each row is its cells joined by 2 spaces in a
// centred paragraph, as vanilla centres its rows. With bRepairBoxGains off, a
// table holds no gains and is laid out as a box with no gains to show.
//
// The gains are drawn at size 14 and the other cells at 18. Where the widest
// row would pass the room, see MessageBox::RowRoom, every row takes these steps
// in order until it fits: the gains' short words, "+2 DMG" for "+2 Damage",
// the other cells 1 size down at a time to 14, then each name still too wide
// cut with "..." to the room the other cells leave it, never under 8 letters.
// Only a box that still passes then leaves out its gains. A row as 1 line
// takes the same steps.
namespace Columns
{
	// 1 box's rows, each its cells in order, the same count on every row. The
	// first kept columns always stay, the rest are the gains by type in their
	// full words, and shortGains holds each row's gains in their short words.
	// name is the column of a row's name, the only one ever cut, or none.
	struct Table
	{
		std::vector<std::vector<std::string>> rows;
		std::vector<std::vector<std::string>> shortGains;
		std::size_t                           kept{ 0 };
		std::optional<std::size_t>            name;
	};

	// The rows as the box takes them, measured in a_movie, the menu under the
	// box. a_box names the box in the trace line that gives the rows as sent.
	// Without a movie, or with a cell measured at nothing, each row is its
	// cells joined by 2 spaces in a centred paragraph.
	[[nodiscard]] std::vector<std::string> Lay(Scaleform::GFx::Movie* a_movie, const Table& a_table, std::string_view a_box);

	// Writes to NEC.log how many boxes left out their gains since the game
	// started, when more have since it last did. main.cpp calls it as a game
	// ends: the main menu opening, a save loading or a new game.
	void SayLeftOut();
}
