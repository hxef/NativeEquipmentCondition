#pragma once

#include "Core/Plugin.h"

#include "UI/Repair/Workbench/Bench.h"

#include <cstdint>
#include <vector>

// A repair at the workbench, weapon or armor, from the question to the finished
// item. Private to this folder.
//
// The game repairs power armor at its station through this same menu class, and
// the weapon and armor benches inherit those functions empty, so a repair goes
// down the game's own path: fill in the job, raise the repairing flag, and hand
// it to the game's own TryCreate by its ID, which prices it, checks the
// components and draws the confirmation. The job stays only if the
// confirmation went up, and yes spends the components. A job the bench cannot
// pay for never gets that far, see Missing.h. The bench refuses to
// price a job with no recipe, so the job carries one of its own, shaped like a
// real recipe, listing the job's components and never registered as a form.
namespace Workbench
{
	using ModChoice = RE::WorkbenchMenuBase::ModChoiceData;
	using PartList = RE::BSTArray<RE::BSTTuple<RE::TESForm*, RE::BGSTypedFormValuePair::SharedVal>>;

	// What the bench is repairing, while it is. One bench is open at a time.
	struct Job
	{
		ModChoice     choice{};
		PartList      parts;
		std::uint32_t level{ 0 };
	};

	[[nodiscard]] Job& InHand();

	// Asks how far to repair the item, naming the perk above the question where
	// the player holds a rank. The answer goes to Begin a moment later.
	void AskWhichLevel(const Selection& a_selection, const std::vector<std::uint32_t>& a_offered);

	// Hands the bench a repair to price, as the power armor station hands
	// itself one. On yes it comes back through BuildConfirmed to Finish.
	void Begin(RE::ExamineMenu* a_menu, std::uint32_t a_level);

	// Writes the chosen condition onto the stack the bench shows, pays
	// experience, spends the components and brings the bench up to date. A free
	// repair is announced in the corner.
	void Finish(RE::ExamineMenu* a_menu);

	// Repairs a barely worn item on the spot for free: the same work a paid
	// repair ends in, handed a job that asks for no components.
	void Mend(RE::ExamineMenu* a_menu);

	// Puts the bench back on mods, whatever became of the repair.
	void Drop(RE::ExamineMenu* a_menu);

	// Says what the bench is looking at: whether the button offers REPAIR or
	// RENAME, and whether the CURRENT MODS heading still applies. Both follow
	// the item, so both are written wherever it or its condition changes.
	void Announce(RE::ExamineMenu* a_menu, const Selection& a_selection);

	// Gives the bench's REPAIR button its word: MEND over an item worn so
	// little that repairing it is free, REPAIR otherwise. Called as the buttons
	// redraw, when the bar is sure to hold the bench's own.
	void Label(RE::ExamineMenu* a_menu, const Selection& a_selection);
}
