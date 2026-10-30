#pragma once

#include "Core/Plugin.h"

// The weapons and armor a patch tells NEC to leave alone, for a mod that gives
// them a condition of its own. A patch ships a file in
// Data/F4SE/Plugins/NEC/Exclude/, such as MyMod.ini, with 1 item a line in
// the form Core/ListFiles.h gives. A listed item takes no condition: NEC never
// rolls, wears or repairs it or changes its stats or price, and never reads,
// writes or clears a health it carries, since that may be another mod's. NEC
// ships no file there.
namespace Excluded
{
	// Reads every .ini file in the folder, finds each line's form in the load
	// order and says in NEC.log what each file leaves alone and why it skipped
	// a line. Writes nothing while the folder is missing or empty. Keeps form
	// IDs only, so Unload lets them go.
	void Load();

	// Forgets the list, so every item takes part again until Load runs.
	void Unload();

	// Whether any file named an item, 1 relaxed load. A caller that walks an
	// inventory to ask Has asks this first.
	[[nodiscard]] bool Any();

	// Whether a_form is on a list. False for every form while no list is read.
	// Safe from any thread, see Core/Snapshot.h.
	[[nodiscard]] bool Has(const RE::TESForm& a_form);
}
