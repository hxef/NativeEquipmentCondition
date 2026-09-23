#pragma once

#include "Core/Plugin.h"

// Where a weapon came from and who carries it. A purely random condition makes
// every raider's gun as worn as every Courser's. Fallout 3 wrote a health onto
// each leveled list entry by hand. Naming factions here would be wrong the
// moment a faction mod appears, so the ranking is read from the load order
// instead, from what a faction's gear is worth and how much armor it gives its
// members. A well funded faction is given good weapons.
//
// 2 halves, measured separately. Supply is how good the gear is that the weapon
// came from: the engine stamps the leveled list a spawned item came from into
// an ExtraLeveledItem before it reaches any inventory, and averaging what that
// list's weapons are worth, ranked against every other weapon list in the load
// order, puts raider pipe lists near the bottom and Institute lists at the top.
// Care is how well the owner keeps their gear: the armor their base form is
// given, outfit and own inventory together, ranked the same way. Power armor
// has no rating to add up, so a character given one counts as the best equipped
// there is. A unique, essential or protected character gets a boost on top.
//
// The 2 are multiplied. Supply is a ceiling and care is how far below it the
// weapon has fallen, so a raider who looted a Gauss rifle still carries a worn
// one. Reading the gun's own value would say the opposite.
//
// Only the outermost list is recorded, so a gun from a list everybody shares
// counts as nothing in particular. Nothing is recorded for a console additem, a
// quest reward, a vendor restock or a gun on a table, and there the owner is
// the only half. Values are read from records, not copies, so mods are not
// counted. A half that cannot be measured counts as the middle.
//
// Supply.cpp is the supply half, Care.cpp the care half, Rank.h where a
// measurement ranks against the rest, and Supply.h and Care.h what each half
// shares with Provenance.cpp.
namespace Provenance
{
	// Reads every leveled list and outfit in the load order and ranks each
	// against the rest. Runs every time game data has loaded.
	void Load();

	// Forgets everything Load measured.
	void Unload();

	// A half that could not be measured. Different from a measured 0, which is
	// the poorest thing in the load order.
	inline constexpr float UNMEASURED = -1.0F;

	// What is known about one weapon arriving in one inventory.
	struct Origin
	{
		// How good the gear is that this weapon came from, from 0 for the
		// poorest source in the load order to 1 for the richest.
		float supply{ UNMEASURED };

		// How well the owner keeps their equipment, on the same scale.
		float care{ UNMEASURED };

		// Where the 2 halves were read from, for the trace log. Empty when the
		// half beside it is UNMEASURED. The list is an ID and not a form, since
		// the ID is what the engine stamped and all the log prints, and looking
		// the form up would take a locked table lookup on a loader thread for
		// every spawn.
		RE::TESFormID pipeline{ 0 };

		// Whoever owns the inventory, when that is a character. Set even when
		// care failed, so the log can tell a character this failed on from an
		// inventory with no owner.
		const RE::TESForm* keeper{ nullptr };

		// The middle of the band this weapon spawns in. An unmeasured half
		// counts as the middle of its scale, so one half missing pulls the
		// answer halfway to the plain average.
		[[nodiscard]] float Centre() const;
	};

	// The lowest and highest Centre can return, the poorest source in the
	// poorest hands and the richest in the best, for the startup line.
	[[nodiscard]] float LowestCentre();
	[[nodiscard]] float HighestCentre();

	// Both halves for one stack on its way into one inventory. Safe before Load
	// has run and with a list that has no owner. Both halves then come back
	// UNMEASURED.
	[[nodiscard]] Origin Of(const RE::BGSInventoryList& a_list, const RE::BGSInventoryItem::Stack& a_stack);
}
