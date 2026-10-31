#pragma once

#include "Core/Plugin.h"

#include "Core/Parts.h"

#include <algorithm>
#include <cstdint>
#include <vector>

// What the folder's files share. Private to this folder.
namespace FoodSpoil
{
	// The fridge list, in FridgeList.cpp. Containers named in a file in
	// Data/F4SE/Plugins/NEC/Fridge/, 1 base form a line in the form
	// Core/ListFiles.h gives. Food in them spoils at fFridgeSpoilSpeed. NEC
	// ships NEC_Default.ini there with the vanilla fridges and coolers.
	void LoadFridges();

	// Forgets the list, so no container is a fridge until LoadFridges runs.
	void UnloadFridges();

	// Whether a_base, a container's base form, is on the list. Safe from any
	// thread, see Core/Snapshot.h.
	[[nodiscard]] bool IsFridge(const RE::TESForm& a_base);

	// Fridges, in Fridge.cpp. Food in a fridge keeps a stamp of its own: a
	// day as far back as makes its days spent so far read the same at
	// fFridgeSpoilSpeed. A store writes it and a take writes the plain stamp
	// back, so the days spent carry over both ways. A slider change makes
	// stored food read more or fewer days spent. IsFridgeRef and InOpenFridge
	// are in FoodSpoil.h, since the fridge icon asks them too.

	// The plain stamp of food stamped a_stamp in a fridge, on a_today.
	[[nodiscard]] float Thaw(float a_stamp, float a_today);

	// The door's rewrite for a stack of a_object going into a fridge,
	// a_into, or out of one, with its plain stamp a_start.
	void Restamp(RE::ExtraDataList& a_extra, const RE::TESBoundObject& a_object, std::uint32_t a_count, float a_start, bool a_into,
		float a_today);

	// Stamps the food on each character or creature that dies, in Bodies.cpp.
	// Once, since the event source lasts as long as the game.
	void WatchBodies();

	// Stamps every food in a_holder's inventory that spoils and has no stamp
	// yet on a_today, with the starting freshness it waited with, and returns
	// how many stacks, in Bodies.cpp. a_died says a_holder just died, so food
	// it carried with none rolls one as found food. Takes the inventory lock
	// and waits for it, so the caller holds none of the engine's locks.
	std::uint32_t StampHeld(RE::TESObjectREFR& a_holder, float a_today, bool a_died);

	// Turns food whose time ran out spoiled as the Pip-Boy, a container or a
	// trade opens, in Sweep.cpp. Once, since UI lasts as long as the game.
	void WatchMenus();

	// a_percent as a share from 0 to 1, kept from 0 to 100 first. For
	// iSpoiledHealing and iSpoiledHunger.
	[[nodiscard]] inline float ShareOf(std::int32_t a_percent)
	{
		return static_cast<float>(std::clamp(a_percent, 0, 100)) / 100.0F;
	}

	// Patches eating, in Eat.cpp: spoiled food gives SpoiledShare of the
	// food's healing and bonuses, and more rads than the food's own. a_group
	// is the part of its places, which go in together.
	void InstallEat(Part a_group);

	// Forgets which food the player last ate spoiled, in Eat.cpp.
	void ForgetEaten();

	// Patches Survival's read of a food's caps value, in Hunger.cpp: spoiled
	// food the player eats in Survival fills iSpoiledHunger % of the hunger
	// and thirst its fresh food fills. a_group is the part of its place.
	void InstallHunger(Part a_group);

	// Notes that the player just ate spoiled a_food in Survival, for
	// Survival's read of its value. Returns whether Hunger.cpp's place runs.
	bool NoteSpoiledMeal(const RE::TESBoundObject& a_food);

	// Forgets every note, for a load or a new game.
	void ForgetMeals();

	// Measures what the rads of spoiled food go through, in Rads.cpp, once
	// LoadKinds ran.
	void LoadRads();

	// Forgets it, so spoiled food adds no rads until LoadRads runs.
	void UnloadRads();


	// Patches cooking and chemistry stations, in Cook.cpp: their rows leave
	// spoiled food out, and a build takes fresh pieces only. a_group is the
	// part of its places, which go in together.
	void InstallCook(Part a_group);

	// The keywords of a cooking and a chemistry station's furniture, by
	// editor ID. Both stations open the cooking menu.
	inline constexpr const char* COOKING = "WorkbenchCooking";
	inline constexpr const char* CHEMISTRY = "WorkbenchChemlab";

	// Whether a_menu is the cooking menu of a bench whose furniture has the
	// keyword a_editorID. The weapon, armor, power armor and robot benches
	// run the same code and answer false.
	[[nodiscard]] bool StationHas(const RE::WorkbenchMenuBase& a_menu, const char* a_editorID);

	// A stack of food in a container a station reaches.
	struct HeldStack
	{
		RE::NiPointer<RE::TESObjectREFR>       holder;
		RE::TESBoundObject*                    food;
		RE::BSTSmartPointer<RE::ExtraDataList> extra;
		std::uint32_t                          count;
		float                                  freshness;
	};

	// The stacks of food that Spoils in a_station's containers, of a_food
	// alone when one is given, in Cook.cpp. Fresh ones on a_today when
	// a_fresh, else the spoiled and the ones whose time ran out. Read at the
	// fridge speed in a fridge, in the containers' order and each chain's
	// own.
	[[nodiscard]] std::vector<HeldStack> StationStacks(const RE::WorkbenchMenuBase& a_station, const RE::TESBoundObject* a_food, bool a_fresh,
		float a_today);

	// Takes up to a_most pieces of a_stack out of its container, and returns
	// how many it took.
	std::uint32_t Take(const HeldStack& a_stack, std::uint32_t a_most);

	// Patches chemistry stations, in Fertilizer.cpp: a Fertilizer row under
	// Utility turns iSpoiledPerFertilizer spoiled food in reach into 1 of the
	// vanilla loose fertilizer. a_group is the part of its places, which go in
	// together.
	void InstallFertilizer(Part a_group);

	// Says in NEC.log what the Fertilizer row makes, once game data has
	// loaded and while its places are in. Keeps nothing.
	void SayFertilizer();

	// Puts the stamped stack of a_item with the earliest day first, in
	// Order.cpp. The caller holds the inventory's write lock. Returns whether
	// a stack moved.
	bool OrderStacks(RE::BGSInventoryItem& a_item);

	// Has the Pip-Boy list a_food again with its CND as of now, through a
	// write that changes nothing on the player's 1st stamped stack of it, in
	// Order.cpp. Takes the inventory lock, so the caller holds none.
	void Relist(RE::TESObjectREFR& a_player, RE::TESBoundObject& a_food);
}
