#pragma once

#include "Core/Plugin.h"

// REPAIR on the Pip-Boy's inspect screen, the one INSPECT opens on a weapon or
// armor row. While iPipboyRepair is 1, REPAIR shows last on the screen's bar
// for a worn item the bench can rebuild while the player carries a spare for
// it, and T, Y on a pad, or a click opens the bench's box of spares, see
// SpareBox.h. While it is 2, every worn item the bench can rebuild shows
// REPAIR, or MEND from fFreeMendFrom up, and the press is the bench's own,
// see BenchPress, paid in components from the player's inventory, since a
// workbench menu with no bench counts only those. A repair keeps the item
// on show, see Finish in Job.h. PREV and NEXT step to other items with the
// screen open, so REPAIR is decided again at every step, after every repair
// and at every press, always for the item on show.
//
// The screen is the workbench menu in inspect mode, so the bench's box code
// and its 3 places behind REPAIR do the repair. The hint is NEC's own, built
// as the game builds its hints and found again by its word on every bar the
// menu has, since the menu's movie keeps no member NEC adds. It leaves with
// the movie at EXIT, so the Pip-Boy's own bar never shows it. Private to this
// folder.
namespace Workbench::Inspect
{
	// Patches the key place, "inspect key", and says in NEC.log what the
	// screen offers as the settings stand. a_benchPatched says whether NEC
	// patches the 3 places behind the bench's REPAIR, since REPAIR here rides
	// on them. It and the key place are read from what the patch calls
	// return, since no link holds before the row's turn ends, see
	// CallPatch::Begin.
	void Install(bool a_benchPatched);

	// Forgets the last inspect screen, for a new workbench menu movie.
	void Forget();

	// Shows or hides REPAIR for the item on show in a Pip-Boy inspect, and
	// says why in the trace. Adds the hint to the bar the first time. Does
	// nothing anywhere else, while NEC is off or while iPipboyRepair is
	// neither 1 nor 2. While a place REPAIR rides on is left to another mod,
	// it hides the hint and offers nothing. True when a row is highlighted
	// but no bar takes REPAIR yet, so the decision waits for one.
	bool Decide(RE::ExamineMenu& a_menu);

	// Called every frame by the bench movie's listener, see Display.cpp, so
	// a fresh inspect screen gets its decision once its list and bar are up.
	// Only 1 bool is read once the screen has one. After 30 frames with a
	// highlighted row and no bar to take REPAIR, T stays vanilla.
	void OnFrame(Scaleform::GFx::Movie& a_movie);

	// Whether NEC acts through the key place now, see CallPatch::LinkBase.
	[[nodiscard]] bool KeyLive();
}
