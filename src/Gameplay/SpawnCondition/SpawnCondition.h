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
	// entries in a plugin, so Install rolls the number in code at the one
	// function every item passes through into an inventory. The ranking those
	// games wrote by hand is measured instead, see Provenance.h, and the roll
	// stays inside the band the measurement asks for.
	//
	// Every item rolls on its own. 3 pistols arriving as one stack with no
	// condition are split into 3, each with its own roll, which happens most on
	// a save made before this mod was installed, see SplitOff in
	// SpawnCondition.cpp.
	//
	// A trader's own stock rolls in a band set by how far the trader repairs
	// it, see VendorRepair/Upkeep.h. A legendary weapon or piece of armor put
	// into any chest arrives new, which covers a trader's showpiece, placed
	// once by a quest script and never restocked. An essential character's
	// weapons and armor, a companion's for example, arrive new.
	//
	// Band.cpp is the roll itself, either side of an item's middle or in a
	// trader's band, Guards.cpp holds KeepCarried and what marks a console
	// command, a save loading, a script giving an item and a restock, and
	// Trace.cpp what the trace log is told about a stack. Band.h, Guards.h and Trace.h are what they share
	// with SpawnCondition.cpp.
	void Install();

	// Sets every weapon and piece of armor the player carries with no
	// condition yet, in a stack that is the player's alone, to full, so what
	// arrived while Worn loot was off stays as it came. Called as Worn loot is
	// switched back on, and does nothing while a save loads.
	void KeepCarried();

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
