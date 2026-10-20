#pragma once

#include "Condition/CraftingPerks/Recipes.h"
#include "Core/Plugin.h"

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// What every recipe says about perks, measured as a whole. Private to this
// folder.
namespace CraftingPerks
{
	// 1 perk and its count: recipes when choosing the perk for a kind of
	// weapon, component units when choosing it for 1 item.
	struct Tally
	{
		const RE::BGSPerk* perk{ nullptr };
		std::uint32_t      count{ 0 };
	};

	// Adds a_count to a_perk's entry in a tally.
	void Note(std::vector<Tally>& a_tally, const RE::BGSPerk* a_perk, std::uint32_t a_count);

	// How many mod recipes 1 perk unlocks across the load order, weapon mods
	// and armor mods counted apart. The total breaks the tie for a weapon that
	// names 2 perks equally, like the Fat Man, half Gun Nut and half Science:
	// Gun Nut unlocks more mod recipes in all than Science, so the Fat Man goes
	// under Gun Nut, where vanilla puts it. NEC.log names each perk's total.
	// The weapon and armor counts say what a perk's page should name.
	struct Gates
	{
		std::uint32_t weapons{ 0 };
		std::uint32_t armor{ 0 };

		[[nodiscard]] bool operator==(const Gates&) const = default;
	};

	// 1 rank's own description, what it takes off a repair, and what its
	// mods go on. perk and rank name it in the trace.
	struct Told
	{
		const RE::TESDescription* description{ nullptr };
		std::uint32_t             percent{ 0 };
		const RE::BGSPerk*        perk{ nullptr };
		std::uint32_t             rank{ 0 };
		bool                      weapons{ false };
		bool                      armor{ false };

		[[nodiscard]] bool operator==(const Told&) const = default;
	};

	struct Perks
	{
		// Every perk in the load order, so a recipe's HasPerk conditions are
		// read at the press, see LaddersOf.
		std::unordered_set<const RE::TESForm*> all;
		// The perk behind most weapon mods of each kind, for weapons whose
		// parts name none. Indexed by the game's weapon type byte, hand to
		// hand at 0 to mine at 11.
		std::array<const RE::BGSPerk*, KINDS> kinds{};
		// The same for armor, which is 1 kind: the perk behind most armor
		// mods, Armorer in vanilla.
		const RE::BGSPerk* armor{ nullptr };
		// What each perk unlocks.
		std::unordered_map<const RE::BGSPerk*, Gates> gates;
		// Every rank of every perk that can price a repair, 15 in vanilla, so
		// a plain loop is fast enough.
		std::vector<Told> told;
	};

	// The counts Load's lines name.
	struct Counted
	{
		std::uint32_t                   gated{ 0 };
		std::uint32_t                   armorGated{ 0 };
		std::uint32_t                   weaponCount{ 0 };
		std::uint32_t                   armorCount{ 0 };
		std::vector<const RE::BGSPerk*> saying;
	};

	// Reads every perk and recipe into new gates, fallbacks and perk page
	// lines. In Perks.cpp.
	[[nodiscard]] std::shared_ptr<Perks> Measure(Counted& a_counted);

	// The perks readers go by, or nothing before Load and after Unload. In
	// CraftingPerks.cpp.
	[[nodiscard]] std::shared_ptr<const Perks> Current();

	// What 1 perk unlocks, looked up without adding an entry.
	[[nodiscard]] Gates GatesOf(const Perks& a_perks, const RE::BGSPerk* a_perk);

	// How many recipes 1 perk unlocks in all.
	[[nodiscard]] std::uint32_t Weight(const Perks& a_perks, const RE::BGSPerk* a_perk);
}
