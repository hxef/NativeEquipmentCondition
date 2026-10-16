#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <functional>
#include <string>

namespace SpawnCondition
{
	// Gives a weapon or a piece of armor a condition the first time it enters
	// anyone's inventory. Fallout 3 and New Vegas handed out worn loot from a
	// health percentage written on every leveled list entry. Fallout 4 still
	// has the system: a list or container entry carries an Item Condition and
	// the engine writes it into the health extra data, but only for a record
	// with a health of its own. No weapon has one. In the base game only power
	// armor pieces and fusion cores do, and they are the only items given an
	// Item Condition. Bringing that back would mean writing thousands of
	// entries in a plugin, so Install rolls the number in code, once, where
	// the game first makes an item: as it enters an inventory, or as its first
	// holder takes it from the world, see World.h. The ranking those games
	// wrote by hand is measured instead, see Provenance.h, and the roll stays
	// inside the band the measurement asks for.
	//
	// A save loading rolls nothing, so what a game holds keeps the condition
	// the save gives it. An item made before the install has none and reads as
	// full, and goes to full the first time it changes hands, see HandOver in
	// Handover.h.
	//
	// Every item rolls on its own. 3 pistols arriving as one stack with no
	// condition are split into 3, each with its own roll, see SplitOff in
	// Roll.cpp.
	//
	// A trader's own stock rolls in a band set by how far the trader repairs
	// it, see VendorRepair/Upkeep.h. A legendary weapon or piece of armor put
	// into any chest arrives new, which covers a trader's showpiece, placed
	// once by a quest script and never restocked. An essential character's
	// weapons and armor, a companion's for example, arrive new, and so does
	// the outfit the player's own record puts on them in a new game.
	//
	// Band.cpp is the roll itself, either side of an item's middle or in a
	// trader's band, Roll.cpp a stack's first condition and its split,
	// Handover.cpp an item changing hands, World.cpp loot lying in the world,
	// Guards.cpp KeepCarried and what marks a save being applied, a console
	// command, a script giving an item and a restock, Owners.cpp the owner
	// checks, and Trace.cpp what the trace log is told about a stack. Their
	// headers are what the folder's files share.
	void Install();

	// Whether KeepCarried may run from the MCM: no main menu, no loading
	// screen, and the player stands in a cell. Asks the UI, so never under an
	// inventory lock.
	bool CanKeepCarried();

	// Sets every weapon and piece of armor the player carries with no
	// condition yet, in a stack that is the player's alone, to full, so what
	// arrived while Worn loot was off stays as it came. Called as Worn loot is
	// switched back on, after CanKeepCarried.
	void KeepCarried();

	// The same once a save has loaded, for what a game in progress picked up
	// before NEC rolled it, a unique quest weapon included. Called before the
	// load's loading screen closes, never for a new game or a coc from the
	// main menu.
	void KeepCarriedAfterLoad();

	// Gives the loose items the trade on this thread bought their mark back,
	// so a respawn puts each back to roll again, see NoteBought in
	// Handover.cpp. Called once the trade has bought every stack, before the
	// trader's side is built again. With a_mark false nothing is written and
	// the list is only emptied.
	void TradeDone(bool a_mark);

	// What a trader's stock of one item rolls in, low to high. worst is the
	// lowest the rare roll that ignores the band can land, and repairs how far
	// the trader repairs the item for the trace log, a whole percent or 0 for
	// not at all.
	struct StockBand
	{
		float         low{ 0.0F };
		float         high{ 0.0F };
		float         worst{ 0.0F };
		std::uint32_t repairs{ 0 };
	};

	// A trader's chest restocking: the chest, by the handle its inventory
	// names its owner with, the trader's name for the trace log, and the band
	// each item rolls in.
	struct Restock
	{
		RE::ObjectRefHandle                                 chest;
		std::string                                         trader;
		std::function<StockBand(const RE::TESBoundObject&)> band;
	};

	// Marks what enters a_restock's chest on this thread, while alive, as that
	// trader's stock. Nested, so an inner restock puts the outer one back.
	class ScopedRestock
	{
	public:
		explicit ScopedRestock(const Restock& a_restock);
		~ScopedRestock();

		ScopedRestock(const ScopedRestock&) = delete;
		ScopedRestock(ScopedRestock&&) = delete;
		ScopedRestock& operator=(const ScopedRestock&) = delete;
		ScopedRestock& operator=(ScopedRestock&&) = delete;

	private:
		const Restock* _was;
	};
}
