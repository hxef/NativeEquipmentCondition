#pragma once

#include "Core/Plugin.h"

#include "UI/Repair/Workbench/Job.h"

// A repair the bench cannot pay for. Private to this folder.
//
// The power armor station answers one with a box: "You lack the requirements
// to repair this item.", a row per component such as "Steel (3/5)", the short
// rows faded, and 1 OK button. The weapon and armor benches are the same menu
// class, so they put up the same box, built the way the station builds it,
// with every word the game's own. Where the box cannot be built, the corner
// says the same title instead.
namespace Workbench
{
	// Puts the station's box up when the bench cannot pay for the job, and
	// says whether it did. Asked before the repair starts, so a repair the
	// bench turns down never starts. False when the bench can pay, and when
	// the box cannot be built, which leaves the corner message to TryCreate.
	// The box reads the job's parts a frame later, as it opens, so they stay
	// until then.
	[[nodiscard]] bool ShowMissing(RE::ExamineMenu* a_menu, const Job& a_job);

	// TryCreate's corner message for a repair it cannot pay for, in the game's
	// repair words. TryCreate shows sCannotBuildMessage, "You lack the
	// requirements to create this item.", which the game reads there and
	// nowhere else. For as long as this object exists, that setting holds the
	// words of sCannotRepairMessage, "You lack the requirements to repair this
	// item.", in the player's language, and it gets its own words back after.
	// The message copies the words as it goes up.
	class ScopedRepairWords
	{
	public:
		ScopedRepairWords();
		~ScopedRepairWords();

		ScopedRepairWords(const ScopedRepairWords&) = delete;
		ScopedRepairWords(ScopedRepairWords&&) = delete;
		ScopedRepairWords& operator=(const ScopedRepairWords&) = delete;
		ScopedRepairWords& operator=(ScopedRepairWords&&) = delete;

	private:
		RE::Setting* _build{ nullptr };
		const char*  _kept{ nullptr };
	};
}
