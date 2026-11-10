#pragma once

#include "Core/Plugin.h"

// The script NEC that other mods call, as NEC.GetCondition(akItem). Its
// functions are global natives bound here to the game's script VM, and
// NEC.pex declares them, built from publish/Scripts/Source/User/NEC.psc.in.
// The game builds a new VM as it starts and after every full reset, and F4SE
// asks NEC to bind again each time, so nothing here keeps a VM.
namespace Papyrus
{
	// Hands the bind to F4SE, which takes it only while the plugin loads.
	void Install();

	// Answers item calls again and asks NEC.pex its version, every time game
	// data has loaded, and NEC.log and NEC.papyrus.log say whether it belongs
	// to this NEC.dll.
	void Load();

	// Answers no item call and drops every waiting event and work, as every
	// form is about to be deleted.
	void Unload();

	// The warnings NEC.papyrus.log has counted and not written yet, as a
	// game ends: a save loads, a new game begins or the main menu opens. The
	// calls waiting for a locked inventory count their tries from 1 again.
	void EndGame();

	// The task that runs a write's work and hands the waiting events to
	// scripts, run once more as a save begins, so no event of a change is
	// left out of the save it is in. The game has stopped its scripts then,
	// and the events reach them as they run again.
	void BeforeSave();
}
