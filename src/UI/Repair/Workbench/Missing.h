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
	// Says whether the bench cannot pay for the job, and then shows what it
	// lacks: the station's box, or its title in the corner where the box
	// cannot be built. Asked before the repair starts, so a repair the bench
	// turns down never starts. The box reads the job's parts a frame later,
	// as it opens, so they stay until then.
	[[nodiscard]] bool ShowMissing(RE::ExamineMenu* a_menu, const Job& a_job);
}
