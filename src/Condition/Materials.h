#pragma once

#include "Core/Plugin.h"

#include <span>

// What a weapon is built from. Fallout 4 has no weapon health and no field
// saying how fast a gun should wear. It does say what every gun is made of: the
// base form plus its mods, and nearly every mod has a recipe listing components
// and counts. Each component has a worth, in vanilla wood 2, steel 3, screws
// 12, aluminum 15, circuitry 25, gold 45, nuclear material 50. So the quality
// of a weapon is the average worth of 1 unit of everything its parts take: a
// pipe gun's standard grip, 1 steel and 1 screw, comes to 7.5, an Institute
// laser's long barrel to 16.8.
//
// That is how the engine works out what a weapon scraps into, in
// ExamineMenu::BuildWeaponScrappingArray: the recipe of every enabled mod, plus
// the weapon's own scrap recipe where it has one. Vanilla gives scrap recipes
// to weapons with few mods, knives and the super sledge into steel, pool cues
// into wood. A combat rifle has none and needs none, since its mods cover the
// whole gun.
//
// None of these numbers are written in the code. Components, their worth and
// the recipes are read from the load order, so another plugin repricing steel
// or adding a weapon is picked up. Only which recipe builds which mod is worked
// out in advance, at Load, since searching every recipe on every shot is far
// too slow. WeaponWear/Rate.cpp turns the quality into a wear rate.
namespace Materials
{
	// Reads every recipe in the load order and remembers which builds each
	// mod, weapon and piece of armor. Runs every time game data has loaded.
	void Load();

	// Forgets everything Load found.
	void Unload();

	// The worth of 1 unit of an average component, the median across every mod,
	// weapon and piece of armor the load order has a recipe for, so an overhaul
	// repricing everything moves the median with it. An item with nothing
	// priced on it counts as this.
	float ReferenceQuality();

	// The average worth of 1 unit of everything an item is built from: its own
	// scrap recipe where it has one, plus the recipes of its enabled mods.
	// a_extra can be null. Mods no recipe builds, which is what legendary
	// effects are, are skipped. An item with nothing priced returns
	// ReferenceQuality.
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
	// calls its bill: the same walk as Quality, without the average. A
	// component 2 recipes ask for comes back once with the counts added. Read
	// fresh each time, so a changed recipe is picked up.
	std::vector<Part> BillOfParts(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra);

	// One component, how many of it, and the recipe that asked for it.
	struct Line
	{
		const RE::BGSConstructibleObject* recipe;
		const RE::BGSComponent*           component;
		std::uint32_t                     count;
	};

	// The same walk with the recipe behind every line kept, for a caller that
	// prices one part of a weapon differently from another, since the recipe
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
