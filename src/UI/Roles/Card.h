#pragma once

#include "UI/Roles/Roles.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The item card a menu shows and its CND row, found by what they hold and
// print. A row is a child whose Label_tf is a display object, and a combined
// damage or resistance row one whose EntryHolder_mc is one too.
namespace Roles::Card
{
	// Where one menu's movie keeps its card, vanilla's place.
	struct Place
	{
		std::string_view movie;   // the movie's file, for the trace
		const char*      path;
		bool             always;  // every screen of the movie has a card
	};

	// The card place of a movie file, or null for a movie that draws no card.
	// Its address holds all session, so a listener can carry it as user data.
	[[nodiscard]] const Place* For(std::string_view a_file);

	// The card at a_place in this movie, when it holds an InfoObj array.
	[[nodiscard]] std::optional<Value> Find(Scaleform::GFx::Movie& a_movie, const Place& a_place);

	// The name of the card's first row, or of its first child where no child
	// is a row. A redraw makes every row anew, so a new name tells it.
	[[nodiscard]] std::string FirstRowName(Value& a_card);

	struct Row
	{
		Value              clip;   // the row printing CND
		std::vector<Value> above;  // the rows CND goes above, nearest first
	};

	// The shown row printing a_label and a_percent, with or without %, the
	// child at data order a_index first, found by its printed text since every
	// entry declares Label_tf and Value_tf. The rows above are the shown rows
	// above it up to the last combined row, or with none, up to the first with
	// no Value_tf. Nothing when no row or 2 rows match, 2 matches warning at
	// once and no match only when a_redrawn.
	[[nodiscard]] std::optional<Row> ConditionRow(Scaleform::GFx::Movie& a_movie, Value& a_card, std::uint32_t a_index,
		std::string_view a_label, double a_percent, bool a_redrawn);
}
