#pragma once

#include "Condition/Materials/Yield.h"
#include "Core/CallPatch/Link.h"
#include "Core/Plugin.h"

#include <cstdint>
#include <vector>

// Worn weapons and armor scrap into fewer components, at a workbench and in
// workshop mode, and a broken one into 1 component. Both build the list of
// what an item scraps into first, show it in the confirm box and on yes give
// that same list, so scaling the list scales what the player gets.
//
// The game adds up each component over the item's recipes, then asks
// ScrapRewardCount once per component for the count, which halves it,
// applies the perks and Scrapper and rounds down. NEC sits on that call and
// hands it the count scaled by the item's condition, see Materials::ScrapIn.
// So a 30% Hunting Rifle's 11 Steel go in as 3 and come out as 1. When every
// count of a worn item comes to 0, the call that builds the list adds 1 of the
// component Materials::LeastPart names for each item it scraps. In workshop
// mode a dropped pile is 1 reference that scraps all its items at once.
//
// At the bench the call sits in the list builder, which only the SCRAP button
// runs, and the item is the bench's highlighted row, see Bench.cpp. In
// workshop mode the call sits in a function that also draws the info card, so
// the 2 calls that build a scrap list note the item first and the count only
// scales while one is noted, see Workshop.cpp.
//
// Stays vanilla: junk, gear at 100% or never worn, loose world gear no holder
// rolled yet, excluded gear and Plain apparel, power armor pieces, and the
// recipes, the halving, the shares and Scrapper of the load order.
namespace Scrapping
{
	// Patches the bench's list and count calls, and the 2 workshop list calls
	// with the workshop's count.
	void Install();

	// -------------------------------------------------------------------
	// What Bench.cpp and Workshop.cpp share
	// -------------------------------------------------------------------

	using CountLink = CallPatch::Link<decltype(&RE::ScrapRewardCount)>;

	// The count of a_form that scrapping a_item at a_percent gives: a_count
	// scaled, then handed on through a_link. Writes the trace line, a_where
	// naming the bench or workshop mode, and adds to the newest Tally.
	std::uint32_t Count(const CountLink& a_link, const char* a_where, const RE::TESBoundObject& a_item, std::uint32_t a_percent,
		RE::TESForm* a_form, std::uint32_t a_count, bool a_applyBase, bool a_skipScalar);

	// What Count gave for 1 list, kept while a list hook builds it on this
	// thread. A list built inside another, as for what is linked to a
	// scrapped object, has its own.
	class Tally
	{
	public:
		Tally();
		~Tally();
		Tally(const Tally&) = delete;
		Tally& operator=(const Tally&) = delete;

		void Add(const RE::TESBoundObject& a_item, std::uint32_t a_percent, const Materials::Tallied& a_count);

		// The scrap item of the component the list gains 1 of for each of
		// a_items, or nothing for a list that gives something or that NEC
		// never scaled. Writes the trace line, a_where naming the bench or
		// workshop mode.
		[[nodiscard]] RE::TESObjectMISC* LeastPart(const char* a_where, std::uint32_t a_items) const;

	private:
		Tally*                          outer;
		const RE::TESBoundObject*       item{ nullptr };
		std::uint32_t                   percent{ 0 };
		std::vector<Materials::Tallied> counts;
	};

	// Patches the bench's list and count calls, both or neither, and says in
	// NEC.log whether they went in.
	void InstallBench();

	// Patches the 2 workshop list calls and the count, all or none, and says
	// in NEC.log whether they went in.
	void InstallWorkshop();
}
