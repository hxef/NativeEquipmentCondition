#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <optional>

// Food that spoils with time, as in Fallout 76, on bFoodSpoils, off by
// default. Food the player or a companion carried spoils, and so does food on
// the dead. Food never carried never spoils. How long a food keeps depends on
// its kind, see Kinds.h: raw food and crops keep fRawFoodDays, cooked food
// fCookedFoodDays, and drinks and packaged food never spoil. In a fridge,
// see IsFridgeRef, food spoils at fFridgeSpoilSpeed, see Fridge.cpp. While
// bNecOn is off nothing is stamped or turns spoiled, and a stamp keeps its
// day, so the food spoils on its usual day once bNecOn is on again.
//
// Food starts to spoil when it is stamped with the day, see Stamp.h: as it
// arrives at the player or a companion, see Arrive, and as a character or
// creature dies, in Bodies.cpp. Found food starts with part of its time
// spent, see SetStart. A stack whose time ran out turns spoiled as
// it arrives anywhere, and in Sweep.cpp as a menu that lists it opens.
// Order.cpp keeps the stack closest to spoiling first, so it is the one a
// row shows and eats. Eat.cpp makes spoiled food heal iSpoiledHealing %,
// nothing in Survival, and give more rads, which Rads.cpp works out by
// fSpoiledRadsMult. Hunger.cpp makes it fill iSpoiledHunger % of the hunger
// in Survival, and Sickness.cpp makes it iSpoiledSicknessMult times as likely
// to make the player sick there. Cook.cpp keeps spoiled food out of cooking
// and chemistry, and Fertilizer.cpp turns it into fertilizer at a chemistry
// station.
//
// Kinds.cpp sorts the food, FoodList.cpp reads a mod's kinds on top, and
// FridgeList.cpp reads the fridge list. FoodLog.cpp writes what they found
// to NEC.food.log. Food.h is what they share.
//
// Eating, Survival's hunger, the station rows and the Fertilizer row are 4
// parts of their own, see Core/Parts.h. Another mod on a place of one turns
// off only that part, and food still spoils.
namespace FoodSpoil
{
	// Patches eating, cooking and chemistry stations, each part on its own.
	void Install();

	// Sorts every food and reads the fridge list, whatever the switch says,
	// so food spoils as soon as the switch is turned on in play. Starts
	// listening for deaths and menus. Writes NEC.food.log while the switch
	// is on, see FoodLog.cpp.
	void Load();

	// Forgets the kinds, the fridges and what NEC.food.log says.
	void Unload();

	// Stamps the food the player and every loaded companion carry with no
	// stamp yet, so it starts fresh now. Called on the engine's task queue as
	// the switch is turned on, after SpawnCondition::CanKeepCarried. Takes
	// each inventory lock and waits for it, so the caller holds none of
	// the engine's locks. Writes NEC.food.log again first.
	void StampCarried();

	// The same on the engine's task queue once a save has loaded, for food
	// carried while the switch was off or before NEC had food spoiling. Writes
	// NEC.food.log only when it was not written since the data load.
	void StampCarriedAfterLoad();

	// Whether a_object spoils now: the switch is on and its kind spoils.
	[[nodiscard]] bool Spoils(const RE::TESBoundObject& a_object);

	// What eating spoiled food is like, for the menus that show it. Each is
	// read live, so a change of difficulty shows at once.

	// Whether Survival is on: the difficulty setting, as its Survival
	// script reads it.
	[[nodiscard]] bool InSurvival();

	// What spoiled food gives of the food's own healing and bonuses:
	// iSpoiledHealing %, and none in Survival.
	[[nodiscard]] float SpoiledShare();

	// Whether eating's places run, in Eat.cpp, so spoiled food gives
	// SpoiledShare and more rads.
	[[nodiscard]] bool EatingRuns();

	// Whether a_extra's stack of a_object would be eaten as spoiled food: it
	// Spoils, and it is spoiled or its time ran out, as FreshnessOf reads it.
	[[nodiscard]] bool SpoiledStack(const RE::TESBoundObject& a_object, RE::ExtraDataList* a_extra);

	// The rads eating spoiled a_food adds on top of its own, as the player
	// would take them now, in Rads.cpp. They go through the food's own
	// harmful Rads effect, or through the one most food uses when it has
	// none, so they follow whatever the food's own rads follow: Lead Belly,
	// the player's rad resistance from food, perks and the difficulty. 0
	// while that effect's conditions keep the rads off or fSpoiledRadsMult
	// is 0.
	[[nodiscard]] float ExtraRads(const RE::MagicItem& a_food);

	// The Rads actor value, found by its editor ID once game data loaded.
	[[nodiscard]] const RE::ActorValueInfo* RadsValue();

	// The share of their strength the player's running effects of a_food
	// give, when the player last ate it spoiled, in Eat.cpp. A meal of the
	// same food takes the place of the effects of the one before. Nothing
	// once the player ate it fresh, after a load or a new game, and while
	// EatingRuns is false.
	[[nodiscard]] std::optional<float> EatenShare(const RE::MagicItem& a_food);

	// Forgets what the player ate, for a load or a new game.
	void ForgetPlay();

	// How much of its time the food a_extra belongs to has left, for its
	// card: 1 before it is stamped, 0 once spoiled or once its time ran out.
	// Read at the fridge speed in the fridge the container screen shows.
	// Nothing for a_object when it does not Spoil.
	[[nodiscard]] std::optional<float> FreshnessOf(const RE::TESBoundObject& a_object, RE::ExtraDataList* a_extra);

	// The same for the food in a_holder's stack a_extra, read at the fridge
	// speed when a_holder is a fridge, for the quick container's rows, which
	// show with no container screen open.
	[[nodiscard]] std::optional<float> FreshnessOf(const RE::TESBoundObject& a_object, RE::ExtraDataList* a_extra,
		const RE::TESObjectREFR* a_holder);

	// What a script reads and writes of food, see Papyrus/Food.cpp.

	// a_form's kind as a number, whatever the switch says: -1 for anything
	// that is not food, 0 never spoils, 1 raw, 2 crop, 3 cooked.
	[[nodiscard]] std::int32_t KindNumber(const RE::TESForm& a_form);

	// The game days food of a_form's kind keeps outside a fridge at the
	// sliders as they stand, whatever the switch says. Nothing for food that
	// never spoils and anything that is not food.
	[[nodiscard]] std::optional<float> DaysFor(const RE::TESForm& a_form);

	// How fast a_food spoils against the ordinary food of this load order,
	// for its card: 1 for food that keeps as many days as most foods do, 2
	// for food that spoils in half the days. Read from the sliders as they
	// stand, the fridge left out. Nothing for food that never spoils and
	// anything that is not food.
	[[nodiscard]] std::optional<float> SpoilShare(const RE::TESBoundObject& a_food);

	// The same as FreshnessOf, read at the fridge speed when a_fridge, with no
	// screen asked, so a trade does not show the trader's food fresh. Under
	// the lock that keeps a_extra.
	[[nodiscard]] std::optional<float> StackFreshness(const RE::TESBoundObject& a_object, RE::ExtraDataList* a_extra, bool a_fridge);

	// Stamps a_extra's food of a_object as far back as leaves it a_share of
	// its time today, from above 0 to 1, with a fridge's own stamp when
	// a_fridge. A spoiled stack and found food lose their health, so the
	// stamp alone says how fresh it is. False before the game has a calendar.
	bool SetFreshness(RE::ExtraDataList& a_extra, const RE::TESBoundObject& a_object, float a_share, bool a_fridge);

	// What SwitchFridge did. kBusy names the holder to wait for: its
	// inventory or the container's own list was locked, or a stack the switch
	// rewrites is shared with it.
	enum class Switched : std::uint8_t
	{
		kDone,          // switched, the stamps inside rewritten
		kAlready,       // in the asked state already, nothing written
		kNotContainer,  // no base form, or one that is no container
		kGone,          // the reference is deleted
		kNotLoaded,     // its cell is not loaded, or it has no extra data list
		kNoCalendar,    // the game has no calendar yet
		kNotRecorded,   // the game records no change right now
		kTaken,         // another mod's time left is on the container
		kBusy,          // a lock was held or a stack is shared, see busy
	};

	struct FridgeSwitch
	{
		Switched                 result{ Switched::kNotContainer };
		const RE::TESObjectREFR* busy{ nullptr };
		std::uint32_t            stacks{ 0 };
	};

	// Switches a_fridge's cooling on when a_on is true and off when it is
	// false, for that 1 container, on the fridge list or not, and rewrites
	// the stamp of each food inside as a store or a take would today, so no
	// freshness jumps. A switch back to what the list says drops the mark.
	// Tries each lock once and never waits, so a script's call runs again
	// while one is held. The game saves the mark with the reference, see
	// FridgeSwitch.cpp.
	[[nodiscard]] FridgeSwitch SwitchFridge(RE::TESObjectREFR& a_fridge, bool a_on);

	// Whether a_extra's food has spoiled, not only run out of time.
	[[nodiscard]] bool IsSpoiled(RE::ExtraDataList* a_extra);

	// Takes a_extra's stamp off and gives it the spoiled health, as running
	// out of time does.
	void Spoil(RE::ExtraDataList& a_extra);

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

	// The door while bNecOn is off, called as Arrive is: only rewrites the
	// stamp of food going into or out of a fridge, since the stamp does not
	// say which kind it is. Stamps nothing new and spoils nothing.
	void Carry(const RE::BGSInventoryList& a_list, const RE::TESBoundObject& a_object, RE::BGSInventoryItem::Stack& a_stack,
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

	// Whether a_ref is a fridge now, in FridgeSwitch.cpp: a container
	// SetFridge switched on, or a container whose base is on the fridge list
	// and that SetFridge has not switched off. Safe from any thread. For a
	// container it reads a_ref's own extra data list as a reader for a
	// moment, by tries for up to MARK_WAIT, and a caller may hold that list
	// already. A read that cannot get the lock answers by the list. The
	// player and an NPC take no lock.
	[[nodiscard]] bool IsFridgeRef(const RE::TESObjectREFR* a_ref);

	// Whether a_extra's stack is in the fridge the container screen shows.
	// The card has no holder, so it asks here.
	[[nodiscard]] bool InOpenFridge(const RE::ExtraDataList* a_extra);

	// Whether the fridge icon shows, beside a fridge's name on the HUD and on
	// the cards of food inside: bNecOn, food spoils and bFridgeIcon are on.
	// Read live, so the page's switches take effect at once.
	[[nodiscard]] bool FridgeIconOn();
}
