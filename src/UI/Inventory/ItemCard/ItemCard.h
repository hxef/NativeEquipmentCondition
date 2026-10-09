#pragma once

#include "Core/Plugin.h"

// A CND row on the item cards, the boxes of stats a menu prints beside an item.
// 4 menus draw them, the Pip-Boy, containers, bartering and the examine menu,
// all with the same ActionScript component, and native code hands the card an
// array of entries such as { text: "$dmg", value: 19 }. The engine fills those
// arrays in 2 functions, one for the Pip-Boy and one for the rest, and Install
// patches the calls to both so the card of an item that wears gets one more
// entry: $ItemInfo_CND, a key the game already carries, and the condition as a
// whole percent.
//
// The card stacks the plain rows from the bottom and puts Damage above them
// afterwards, so no position in the array lifts a plain row over Damage.
// OnMovieLoaded listens in each menu's movie for the render event and moves a
// freshly drawn CND row up past Damage.
//
// The same 2 patches tell the card which copy of a weapon it is about, for the
// fire rate: the game works a card's rate out from the weapon and its mods
// alone, so a worn automatic would show the rate of a new one. While FireRate
// slows worn weapons, Install hooks those calls too, and the 2 lists that
// compare weapons by rate without a card, the quick container's better mark and
// a container's sort.
//
// Hooks.cpp is the CND row, Rate.cpp the fire rate, Raise.cpp moves the row
// past Damage, and Cards.h is what they share.
namespace ItemCard
{
	// Patches the calls that fill item cards.
	void Install();

	// Adds the render listener to a menu movie that draws item cards and
	// ignores the rest.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);

	// Whether the Pip-Boy's cards get NEC's row, and with it a worn out item's
	// faded name. False while either Pip-Boy site is left to another mod or
	// waits.
	[[nodiscard]] bool PipboyCards();
}
