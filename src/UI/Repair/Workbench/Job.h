#pragma once

#include "Core/Plugin.h"

#include "UI/Repair/Workbench/Bench.h"
#include "UI/Repair/Workbench/Cost.h"

#include <cstdint>
#include <vector>

// A repair at the workbench, weapon or armor, from the question to the finished
// item. Private to this folder.
//
// The game repairs power armor at its station through this same menu class, and
// the weapon and armor benches inherit those functions empty. So NEC fills in
// the job, asks the game whether the bench can pay for it, see Missing.h, and
// puts the confirmation up itself, see Box.h. Yes spends the components. The
// game refuses to check a job with no recipe, so the job carries one of its
// own, shaped like a real recipe, listing the job's components and never
// registered as a form.
namespace Workbench
{
	using ModChoice = RE::WorkbenchMenuBase::ModChoiceData;
	using PartList = RE::BSTArray<RE::BSTTuple<RE::TESForm*, RE::BGSTypedFormValuePair::SharedVal>>;

	// What the bench is repairing, while it is. One bench is open at a time.
	// The parts stay at this one address, since the boxes read them as they
	// open, a frame after they go up.
	struct Job
	{
		ModChoice     choice{};
		PartList      parts;
		std::uint32_t level{ 0 };
		// The callback of the box asking about this job, see Box.h, and
		// nothing while no box is.
		const RE::ExamineConfirmMenu::ICallback* box{ nullptr };
	};

	[[nodiscard]] Job& InHand();

	// Asks how far to repair the item, naming the perk above the question where
	// the player holds a rank. The answer goes to Begin a moment later, with
	// a_priced, what PriceOf read at the REPAIR press. With a_spare, USE A
	// SPARE sits above the levels and opens the box of spares instead, see
	// SpareBox.h.
	void AskWhichLevel(const Selection& a_selection, const Priced& a_priced, const std::vector<std::uint32_t>& a_offered,
		bool a_spare);

	// Prices the repair from a_priced and puts up its confirmation, or the box
	// of what the bench lacks. Yes on the confirmation comes back to Finish.
	void Begin(RE::ExamineMenu* a_menu, std::uint32_t a_level, const Priced& a_priced);

	// Writes the chosen condition onto the stack the bench shows, pays
	// experience, spends the components and brings the bench up to date. A free
	// repair is announced in the corner.
	void Finish(RE::ExamineMenu* a_menu);

	// Repairs a barely worn item on the spot for free: the same work a paid
	// repair ends in, handed a job that asks for no components.
	void Mend(RE::ExamineMenu* a_menu);

	// Makes a_parts the job's parts, which its recipe lists.
	void SetParts(const std::vector<Materials::Part>& a_parts);

	// What a repair paid in experience, for the trace.
	struct Paid
	{
		std::uint32_t worth{ 0 };
		float         experience{ 0.0F };
	};

	// Pays the experience crafting a mod from the job's parts pays, nothing
	// for no parts.
	Paid PayExperience(RE::PlayerCharacter& a_player);

	// Brings the bench up to date after a repair of an item of a_type: the
	// name in the list, the CND row on the card, the slot list ungreyed once
	// the item reaches the floor, the button, and the Pip-Boy's cards of
	// that kind. The list keeps the highlight where the menu's own record of
	// the highlighted stack says, see SpareBox.cpp.
	void Refresh(RE::ExamineMenu* a_menu, RE::ENUM_FORM_ID a_type);

	// Ends the job, whatever became of the repair. Touches nothing but the
	// job, since it also runs while the bench closes.
	void Drop();
}
