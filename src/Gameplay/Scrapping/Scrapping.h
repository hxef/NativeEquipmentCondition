#pragma once

#include "Core/CallPatch/Link.h"
#include "Core/Plugin.h"

#include <cstdint>

// Worn weapons and armor scrap into fewer components, at a workbench and in
// workshop mode, and broken ones into none. Both build the list of what an
// item scraps into first, show it in the confirm box and on yes give that same
// list, so scaling the list scales what the player gets.
//
// The game adds up each component over the item's recipes, then asks
// ScrapRewardCount once per component for the count, which halves it,
// applies the perks and Scrapper and rounds down. NEC sits on that call and
// hands it the count scaled by the item's condition, see Materials::ScrapIn.
// So a 30% Hunting Rifle's 11 Steel go in as 3 and come out as 1.
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
	// Patches the bench's count call, and the 2 workshop list calls with the
	// workshop's count.
	void Install();

	// -------------------------------------------------------------------
	// What Bench.cpp and Workshop.cpp share
	// -------------------------------------------------------------------

	using CountLink = CallPatch::Link<decltype(&RE::ScrapRewardCount)>;

	// The count of a_form that scrapping a_item at a_percent gives: a_count
	// scaled, then handed on through a_link. Writes the trace line, a_where
	// naming the bench or workshop mode.
	std::uint32_t Count(const CountLink& a_link, const char* a_where, const RE::TESBoundObject& a_item, std::uint32_t a_percent,
		RE::TESForm* a_form, std::uint32_t a_count, bool a_applyBase, bool a_skipScalar);

	// Patches the bench's count call and says in NEC.log whether it went in.
	void InstallBench();

	// Patches the 2 workshop list calls and the count, all or none, and says
	// in NEC.log whether they went in.
	void InstallWorkshop();
}
