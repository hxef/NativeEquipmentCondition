#pragma once

#include "Core/Plugin.h"

#include "Condition/Condition.h"

#include <span>

// What an item is built from. Fallout 4 has no item health and no field saying
// how fast a gun or a piece of armor should wear. It does say what every item
// is made of: the base form plus its mods, and nearly every mod has a recipe
// listing components and counts. Each component has a worth, in vanilla wood 2,
// steel 3, cloth 4, leather 10, screws 12, aluminum 15, circuitry 25, gold 45,
// nuclear material 50. So the quality of an item is the average worth of 1 unit
// of everything its parts take: a pipe gun's standard grip, 1 steel and 1
// screw, comes to 7.5, an Institute laser's long barrel to 16.8, a suit's 2
// cloth to 4.
//
// That is how the engine works out what an item scraps into, in
// ExamineMenu::BuildWeaponScrappingArray: the recipe of every enabled mod, plus
// the item's own scrap recipe where it has one. Vanilla gives scrap recipes to
// weapons with few mods, knives and the super sledge into steel, pool cues into
// wood, and to most wearables, a suit into 2 cloth, a helmet into 2 steel, a
// leather chest piece into 2 leather. A combat rifle has none and needs none,
// since its mods cover the whole gun. A wearable with none, which is every
// piece of Nuka-World's raider armor, half the vault suits and Maxson's
// battlecoat, borrows the recipe the load order gives most often to pieces of
// its kind, clothing or armor, see ArmorWear::IsClothing: 2 cloth for clothing
// and 2 steel for armor in vanilla. So it is priced and wears like similar
// pieces, where the game itself would scrap it into nothing. A weapon whose own
// recipe and mods ask for nothing, Grognak's Axe for one, borrows the recipe
// weapons have most often, 2 steel in vanilla, and so is priced and wears like
// them too.
//
// None of these numbers are written in the code. Components, their worth and
// the recipes are read from the load order, so another plugin repricing steel
// or adding a weapon is picked up. Only which recipe builds which mod is worked
// out in advance, at Load, since searching every recipe on every shot is far
// too slow. WeaponWear/Rate.cpp and ArmorWear/Rate.cpp turn the quality into a
// wear rate.
namespace Materials
{
	// Reads every recipe in the load order and remembers which builds each
	// mod, weapon and wearable, and what an item with nothing of its own
	// borrows. Runs every time game data has loaded.
	void Load();

	// Forgets everything Load found.
	void Unload();

	// Measures the 2 medians of ReferenceQuality again from what each
	// component is worth now, at every save load and new game, and says only
	// what moved. The recipes stay the ones Load found.
	void MeasureAgain();

	// The worth of 1 unit of an average component for an item of a_kind, the
	// median of the recipes of that kind, so an overhaul repricing everything
	// moves the median with it. For weapons those are the recipes that build a
	// weapon or a weapon mod, 19 in vanilla, since a gun's mods are the gun.
	// For armor they are the scrap recipes of the pieces alone, 4 in vanilla,
	// since a piece's own recipe is the piece and its mods are upgrades on top:
	// a plain piece wears at the ordinary rate and an upgraded one lasts
	// longer. An item still with nothing priced, the borrowed recipe included,
	// counts as this.
	float ReferenceQuality(Condition::Kind a_kind);

	// The average worth of 1 unit of everything an item is built from: its own
	// scrap recipe where it has one, the borrowed one for a wearable without,
	// plus the recipes of its enabled mods, and the borrowed one for a weapon
	// where those ask for nothing. a_extra can be null. Mods no recipe builds
	// are skipped, and so is the 1 legendary effect the game names for the
	// item, even where a plugin gives it a recipe. An item still with nothing
	// priced, the borrowed recipe included, returns ReferenceQuality.
	float Quality(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra);

	// One component, and how many of it something takes.
	struct Part
	{
		const RE::BGSComponent* component;
		std::uint32_t           count;

		// So that 2 bills can be compared.
		[[nodiscard]] bool operator==(const Part&) const = default;
	};

	// Everything an item is built from, component by component, which the code
	// calls its bill: the same walk as Quality without the average. A component
	// 2 recipes ask for comes back once with the counts added. Read fresh each
	// time, so a changed recipe is picked up.
	std::vector<Part> BillOfParts(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra);

	// One component, how many of it, and the recipe that asked for it.
	struct Line
	{
		const RE::BGSConstructibleObject* recipe;
		const RE::BGSComponent*           component;
		std::uint32_t                     count;
	};

	// The same walk with the recipe behind every line kept, for a caller that
	// prices one part of an item differently from another, since the recipe
	// says which perk unlocks the part. Lines merge within a recipe and never
	// across 2, so a frame and a scope that both want steel give 2 steel lines.
	std::vector<Line> BillOfLines(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra);

	// Adds the lines up: recipes dropped, the counts of each component added
	// together.
	std::vector<Part> BillOfParts(std::span<const Line> a_bill);

	// The bill laid out 1 unit at a time, mixed in proportion, so the first n
	// entries are what n units of the gun are made of. A component with 11
	// units places its first unit at 0.5/11 and its second at 1.5/11, and all
	// the places are sorted, so 11 aluminum and 3 glass come up in about that
	// proportion all the way down. Ties go to the bigger count, then the lower
	// form ID, so a gun is always laid out the same way. a_units can be longer
	// than the bill, since a repair can cost more than 1 copy of an item's
	// components.
	std::vector<const RE::BGSComponent*> Order(std::span<const Part> a_bill, std::uint32_t a_units);
}
