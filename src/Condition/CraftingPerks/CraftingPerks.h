#pragma once

#include "Condition/Materials.h"
#include "Core/Plugin.h"

#include <span>

// Which crafting perk prices the repair of a weapon, and which rank of it the
// player has. Gun Nut unlocks ballistic mods, Science energy, Blacksmith melee,
// and a load order may add more. Someone who can build a better receiver can
// also fix a worn gun for less, so a repair falls from 2 times the weapon's
// components down to 1 as the player takes more ranks, and every rank's
// description gets a line saying how much it takes off: 13% a rank on a 4 rank
// perk, 17% on a 3 rank one, 50% at the top rank of either.
//
// One repair is priced by one perk, chosen by counting components. A bill has
// one line per component per recipe, and every recipe names the perk that
// unlocks its part, so the perk behind the most component units prices the
// whole repair. A combat rifle with a Science scope stays Gun Nut, a few units
// of glass against 160 of Gun Nut parts. Components are counted, not mods, so a
// sling cannot outweigh a receiver. Rebuilding a gun with parts of a cheaper
// perk gains nothing, since the bench opens no mod slot below full condition.
//
// A weapon whose bill names no perk, the Cryolator, the pool cue or a weapon
// pack with no recipes, falls back on its kind: the perk behind most of the
// mods that kind can take, matched through attach points, which a mod and a
// weapon both name with the same keywords. In vanilla with every DLC, Gun Nut
// wins guns by 370 recipes to Science's 218, and Blacksmith wins every melee
// kind. A slot no weapon offers is skipped, which keeps armor out without
// naming it. A perk from another mod wins once its recipes outnumber the
// others, and Demolition Expert never wins, since it only unlocks grenades and
// mines.
//
// A ladder is a perk and all its ranks, read the way the game reads it: each
// rank is a separate perk record, linked in a ring by next, so Gun Nut 4 points
// back to Gun Nut 1. The first rank asks for the lowest level, and the player's
// rank is the last one in the ring they have. A rebuilt weapon's kind is read
// from its instance data.
//
// Ladder.cpp walks a ring of ranks, Recipes.cpp reads which perks a recipe
// needs and what kind a weapon is, and Description.cpp adds the line to the
// perk page.
namespace CraftingPerks
{
	// Patches the perk descriptions, so every rank says how much it takes off a
	// repair.
	void Install();

	// Reads every recipe and works out which perk each kind of weapon falls
	// back on. Runs after Materials::Load, which reads the same recipes.
	void Load();

	// Forgets every perk and recipe Load found.
	void Unload();

	// What repairing a broken item costs with no rank of the perk, as a
	// multiple of its components. 2, so a broken gun is better scrapped than
	// repaired, and keeping a gun in shape is always cheaper than buying it
	// back.
	inline constexpr float UNSKILLED_MULTIPLE = 2.0F;

	// What the same repair costs at the perk's top rank. The multiple falls in
	// equal steps across the ranks, so each of 4 ranks takes a quarter of the
	// difference. At the top a broken item still costs 1 copy of its
	// components, which keeps scrapping worth doing.
	inline constexpr float SKILLED_MULTIPLE = 1.0F;

	// Which perk covers one weapon and where the player stands on it.
	struct Standing
	{
		// The first rank of the perk, or nothing where no perk covers this
		// weapon, which only happens in a load order with no weapon mod
		// recipes.
		const RE::BGSPerk* perk{ nullptr };

		// How many ranks the player holds, and how many there are.
		std::uint32_t rank{ 0 };
		std::uint32_t ranks{ 0 };

		// True where the perk came from the weapon's kind and not from its
		// parts.
		bool fromKind{ false };

		// The share of the perk's ranks the player holds, 0 to 1, and 0 with no
		// perk at all.
		[[nodiscard]] float Share() const
		{
			return ranks > 0 ? static_cast<float>(rank < ranks ? rank : ranks) /
			                       static_cast<float>(ranks)
			                 : 0.0F;
		}

		// The name the player knows it by, for the log.
		[[nodiscard]] std::string Name() const;
	};

	// Which perk prices one weapon, from a_bill, its list of components: the
	// perk behind the most component units wins. An empty bill, or one naming
	// no perk, falls back on the weapon's kind. a_extra can be null, and then
	// the base form says what kind of weapon this is.
	[[nodiscard]] Standing Of(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra,
		std::span<const Materials::Line> a_bill);

	// Where the player stands on the perk a recipe names in its condition. None
	// for a recipe naming no perk. Where it names 2, which many weapon mods do,
	// the one the player has taken furthest counts.
	[[nodiscard]] Standing OfRecipe(const RE::BGSConstructibleObject& a_recipe);

	// What repairing a broken weapon costs the player at this rank, between the
	// 2 multiples. No perk at all costs the full multiple.
	[[nodiscard]] float Multiple(const Standing& a_standing);

	// What one rank takes off a repair, in whole percent, for its description:
	// 13 a rank on a 4 rank perk, 17 on a 3 rank one, 50 at the top of either.
	[[nodiscard]] std::uint32_t Discount(std::uint32_t a_rank, std::uint32_t a_ranks);
}
