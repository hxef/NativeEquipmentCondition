#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <optional>
#include <string_view>

// NEC.papyrus.log, beside NEC.log and always written: each time the game
// builds its script VM, the scripts each event reached first in a game, and
// the errors in scripts' calls. A warning is written the first time it comes
// for a function, then at most once each 10 s with how many came since its
// last line, so a script calling in a loop cannot flood the file. Nothing here
// is called under an inventory lock, since a line can wait on the disk. Every
// event and every call but the version and settings functions go to
// NEC.papyrus.trace.log, see Core/TraceLog.h. Private to this folder.
namespace Papyrus::Log
{
	enum class Warning : std::uint8_t
	{
		kNone,       // None passed where an item is needed
		kGone,       // a handle to a stack that no longer exists
		kUntracked,  // a write on an item NEC does not change
		kRefused,    // a value the function does not take
		kClamped,    // a value past what the function takes, kept at its end
		kLoading,    // called while a save loads
		kFrozen,     // called while the game froze its scripts
		kEventBusy,  // an event's inventory busy for 2 s, so its item is None
		kDropped,    // events dropped as the queue was full
	};

	// The function this thread runs for a script, which its warnings name.
	// Each function of the script names itself first, by a literal, and that
	// hands on the events that wait, see NEC.psc.in.
	void Calling(std::string_view a_function);

	// a_count more of a_warning for the function this thread runs, and for
	// an event's warning for a_event. a_detail goes into the sentence: the
	// item, the holder, or what was given and used.
	void Warn(Warning a_warning, std::string_view a_detail = {}, std::uint32_t a_count = 1);
	void WarnFor(std::string_view a_event, Warning a_warning, std::string_view a_detail = {}, std::uint32_t a_count = 1);

	// Keeps this thread's warnings back from Hold to Release, which writes
	// them with a_keep and drops them without. A try of a script's call that
	// the VM runs again drops its own, see Waiting.h, so a call counts each
	// warning once.
	void Hold();
	void Release(bool a_keep);

	// The bind of the VM being built: a_functions bound, a_tasklets of them
	// answering at once. NEC.log and the trace log say so now, and
	// NEC.papyrus.log once NEC.pex has answered.
	void Bound(std::uint32_t a_functions, std::uint32_t a_tasklets);

	// NEC.pex's GetScriptVersion answer, none when the call failed, and
	// NEC.dll's version as 1 number. Writes the VM's block.
	void Checked(std::optional<std::int32_t> a_pex, std::int32_t a_dll);

	// a_event queued for a_scripts, "NECTest.OnBroke" and so on, said the
	// first time in each game.
	void Reached(std::string_view a_event, std::string_view a_scripts);

	// The counts not written yet, as a game ends, and every warning counts
	// from 1 again.
	void EndGame();
}
