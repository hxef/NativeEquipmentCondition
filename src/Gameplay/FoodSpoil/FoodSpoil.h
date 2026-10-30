#pragma once

#include "Core/Plugin.h"

#include <optional>

// Food that spoils with time, as in Fallout 76, on bFoodSpoils, off by
// default. Food the player or a companion carried spoils, and so does food on
// the dead. Food never carried never spoils. How long a food keeps depends on
// its kind, see Kinds.h: raw food and crops keep fRawFoodDays, cooked food
// fCookedFoodDays, and drinks and packaged food never spoil. In a container
// on the fridge list, food spoils at fFridgeSpoilSpeed, see Fridge.cpp.
//
// Food starts to spoil when it is stamped with the day, see Stamp.h: as it
// arrives at the player or a companion, see Arrive, and as a character or
// creature dies, in Bodies.cpp. Found food starts with part of its time
// spent, see SetStart. A stack whose time ran out turns spoiled as
// it arrives anywhere, and in Sweep.cpp as a menu that lists it opens.
// Order.cpp keeps the stack closest to spoiling first, so it is the one a
// row shows and eats. Eat.cpp makes a spoiled bite heal a tenth and give
// rads. Cook.cpp keeps spoiled food out of cooking and chemistry, and
// Fertilizer.cpp turns it into fertilizer at a chemistry station.
//
// Kinds.cpp sorts the food, FoodList.cpp reads a mod's kinds on top, and
// FridgeList.cpp reads the fridge list. Food.h is what they share.
//
// The bite, the station rows and the Fertilizer row are 3 parts of their own,
// see Core/Parts.h. Another mod on a place of one turns off only that part,
// and food still spoils.
namespace FoodSpoil
{
	// Patches eating, cooking and chemistry stations, each part on its own.
	void Install();

	// Sorts every food and reads the fridge list, whatever the switch says,
	// so food spoils as soon as the switch is turned on in play. Starts
	// listening for deaths and menus.
	void Load();

	// Forgets the kinds and the fridges.
	void Unload();

	// Stamps the food the player and every loaded companion carry with no
	// stamp yet, so it starts fresh now. Called on the engine's task queue as
	// the switch is turned on, after SpawnCondition::CanKeepCarried. Takes
	// each inventory lock and waits for it, so the caller holds none of
	// the engine's locks.
	void StampCarried();

	// The same on the engine's task queue once a save has loaded, for food
	// carried while the switch was off or before NEC had food spoiling.
	void StampCarriedAfterLoad();

	// Whether a_object spoils now: the switch is on and its kind spoils.
	[[nodiscard]] bool Spoils(const RE::TESBoundObject& a_object);

	// How much of its time the food a_extra belongs to has left, for its
	// card: 1 before it is stamped, 0 once spoiled or once its time ran out.
	// Read at the fridge speed in the fridge the container screen shows.
	// Nothing for a_object when it does not Spoil.
	[[nodiscard]] std::optional<float> FreshnessOf(const RE::TESBoundObject& a_object, RE::ExtraDataList* a_extra);

	// Whether food arriving in a_list starts to spoil there: the player's, a
	// companion's or a dead body's.
	[[nodiscard]] bool Starts(const RE::BGSInventoryList& a_list);

	// Found food: food from the ground, a box, a body placed dead or a
	// character that died starts with a share of its time already spent,
	// which SpawnCondition rolls while Worn loot is on too. Until the food is
	// stamped the share waits on its stack as its starting freshness, so it
	// never spoils where it lies and its card already shows it. The stamp
	// then starts that much of its time back.

	// Whether a_extra's food has no stamp, no starting freshness and is not
	// spoiled, so a roll may give it one.
	[[nodiscard]] bool Unrolled(RE::ExtraDataList& a_extra);

	// Gives a_extra's food a_freshness of its time as its starting
	// freshness, never 0 and never quite 1.
	void SetStart(RE::ExtraDataList& a_extra, float a_freshness);

	// Takes a_extra's starting freshness off, so the food starts fresh.
	// Returns whether it had one.
	bool Freshen(RE::ExtraDataList& a_extra);

	// The door, for a_stack of a_object that Spoils, as AddStack is about to
	// add it to a_list, under that list's write lock. Called only for a
	// stack the add owns, never a copy of another inventory's stack or a
	// save's. a_from is the container a handover takes it out of, if any.
	// Stamps food arriving at the player, a companion or a dead body, with
	// the starting freshness it waited with, rewrites the stamp of food
	// going into or out of a fridge, and turns a stack whose time ran out
	// spoiled, so it joins the spoiled stack.
	// Returns whether a_stack goes in stamped, so Order runs once it is in.
	bool Arrive(const RE::BGSInventoryList& a_list, const RE::TESBoundObject& a_object, RE::BGSInventoryItem::Stack& a_stack,
		const RE::TESObjectREFR* a_from);

	// The door's other side, for a_stack of a_object as AddStack is about to
	// add it to a_list: whether a cooking or chemistry station is filling
	// a_list, the copy its rows count from, and a_stack is spoiled or out of
	// time. The caller then leaves a_stack out. Writes nothing, since the
	// copy shares its extra data with the real stack.
	[[nodiscard]] bool LeftOut(const RE::BGSInventoryList& a_list, const RE::TESBoundObject& a_object,
		const RE::BGSInventoryItem::Stack& a_stack);

	// Right after AddStack, still under the lock: puts the stamped stack of
	// a_object closest to spoiling first in a_list, the stack a row shows
	// and eats.
	void Order(RE::BGSInventoryList& a_list, const RE::TESBoundObject& a_object);

	// Whether a_ref is a container whose base is on the fridge list, in
	// Fridge.cpp. Safe from any thread.
	[[nodiscard]] bool IsFridgeRef(const RE::TESObjectREFR* a_ref);

	// Whether a_extra's stack is in the fridge the container screen shows.
	// The card has no holder, so it asks here.
	[[nodiscard]] bool InOpenFridge(const RE::ExtraDataList* a_extra);

	// Whether the fridge icon shows, beside a fridge's name on the HUD and on
	// the cards of food inside: food spoils and bFridgeIcon is on. Read live,
	// so the page's switches take effect at once.
	[[nodiscard]] bool FridgeIconOn();
}
