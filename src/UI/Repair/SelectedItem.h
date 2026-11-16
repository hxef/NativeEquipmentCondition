#pragma once

#include "Core/Plugin.h"

#include "Condition/Condition.h"
#include "Condition/Repair.h"

#include <cstdint>
#include <string>

// The item highlighted in a menu's inventory list, a weapon or a piece of
// armor. A menu keeps its rows as inventory handles, so a row is turned back
// into an item the way the vanilla price lookup does, through the inventory
// interface. Only an item that wears is read.
namespace SelectedItem
{
	struct Item
	{
		const RE::BGSInventoryItem* item{ nullptr };
		RE::TESBoundObject*         object{ nullptr };
		const RE::ExtraDataList*    extra{ nullptr };
		std::uint32_t               handle{ 0 };
		std::uint32_t               stack{ 0 };
		std::uint32_t               count{ 0 };
		// The number its card shows, in tenths, see Condition::CardTenths.
		std::uint32_t               tenths{ Condition::FULL_TENTHS };
		// tenths / 10, rounded down, so 97.5 is 97. Every compare with a
		// whole point uses it, since tenths is 10 W or more exactly when
		// percent is W or more. At CND decimals Off it is the whole number
		// the card shows.
		std::uint32_t               percent{ Repair::FULL };
		Condition::Kind             kind{ Condition::Kind::kWeapon };

		// Sets tenths, and percent from it.
		void SetTenths(std::uint32_t a_tenths)
		{
			tenths = a_tenths;
			percent = a_tenths / Condition::TENTHS_PER_PERCENT;
		}

		// True for an item below full condition.
		[[nodiscard]] bool Worn() const { return object && percent < Repair::FULL; }

		// True for a piece of armor, for the sentences and the cards that go
		// by kind.
		[[nodiscard]] bool Armor() const { return kind == Condition::Kind::kArmor; }

		// The name the player sees, which is rarely the name on the form: a
		// pipe gun's form is named Pipe and its mods add the rest of the name.
		// The engine's buffer is copied, not pointed at.
		[[nodiscard]] std::string Name() const;
	};

	// One row of a menu's inventory list, turned back into an item.
	[[nodiscard]] Item Read(const RE::InventoryUserUIInterfaceEntry& a_entry);

	// The name a row of a menu's inventory list shows, of any item, also one
	// that never wears, such as a Stimpak.
	[[nodiscard]] std::string RowName(const RE::InventoryUserUIInterfaceEntry& a_entry);
}
