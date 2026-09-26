#pragma once

#include "Core/Plugin.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

// Reading the load order's recipes, what each one's mod could go on, and a
// weapon's kind. Private to this folder.
namespace CraftingPerks
{
	// How many kinds of weapon a table by kind has room for.
	inline constexpr std::size_t KINDS = 16;

	// The perks a recipe needs, from its HasPerk conditions, nearly always 1. A
	// condition names a rank, Gun Nut 2, so each is turned into its first rank,
	// which also keeps 2 ranks of one perk from counting twice. The parameter
	// is checked against a_perks, every perk in the load order, since a
	// condition of another kind holds something else there.
	void LaddersOf(const RE::BGSConstructibleObject& a_recipe,
		const std::unordered_set<const RE::TESForm*>& a_perks,
		std::vector<const RE::BGSPerk*>&              a_out);

	// The slot a mod goes in, or a weapon offers. Both hold it as an index into
	// the game's list of attach point keywords, so the number means the same on
	// both sides. A weapon does not store its mod association keywords the same
	// way, so those are not used.
	[[nodiscard]] const RE::BGSKeyword* PointOf(std::uint16_t a_index);

	// What kind of weapon it is, from the instance data if the weapon was
	// rebuilt and from the base form otherwise.
	[[nodiscard]] std::size_t KindOf(const RE::TESObjectWEAP& a_weapon, const RE::ExtraDataList* a_extra);

	// Which kinds of weapon offer each slot, a bit per kind, and which slots a
	// piece of armor offers, read off every weapon and piece that wears out. A
	// slot nothing offers never appears, which keeps power armor and creature
	// hides out of the count without naming them.
	struct Slots
	{
		std::unordered_map<const RE::BGSKeyword*, std::uint32_t> weapons;
		std::unordered_set<const RE::BGSKeyword*>                armor;

		// How many weapons and pieces of armor were read.
		std::uint32_t weaponCount{ 0 };
		std::uint32_t armorCount{ 0 };
	};

	[[nodiscard]] Slots SlotsOffered();

	// Which kinds of weapon the mod a recipe builds could be attached to, a bit
	// per kind, and whether it could go on a piece of armor. A recipe building
	// a list of mods counts everything any of them can go on.
	struct Reach
	{
		std::uint32_t kinds{ 0 };
		bool          armor{ false };
	};

	[[nodiscard]] Reach ReachOf(const RE::TESForm& a_built, const Slots& a_slots);
}
