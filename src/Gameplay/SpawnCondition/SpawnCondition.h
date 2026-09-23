#pragma once

#include "Core/Plugin.h"

namespace SpawnCondition
{
	// Gives a weapon a condition the first time it enters anyone's inventory.
	// Fallout 3 and New Vegas handed out worn loot from a health percentage
	// written on every leveled list entry. Fallout 4 still has the system: a
	// list or container entry carries an Item Condition and the engine writes it
	// into the health extra data, but only for a record with a health of its
	// own. No weapon has one. In the base game only power armor pieces and
	// fusion cores do, and only they are given an Item Condition. Bringing that
	// back would mean writing thousands of entries in a plugin, so Install
	// rolls the number in code at the one function every item passes through
	// into an inventory. The ranking those games wrote by hand is measured
	// instead, see Provenance.h, and the roll stays inside the band the
	// measurement asks for.
	//
	// Every weapon rolls on its own. 3 pistols arriving as one stack with no
	// condition are split into 3, each with its own roll, which happens most on
	// a save made before this mod was installed, see SplitOff in
	// SpawnCondition.cpp.
	//
	// Band.cpp is how far either side of its middle a weapon rolls, and
	// Guards.cpp what marks a console command, a save loading and a script
	// giving an item. Band.h and Guards.h are what they share with
	// SpawnCondition.cpp.
	void Install();
}
