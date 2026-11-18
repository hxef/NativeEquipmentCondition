#pragma once

#include "Core/Plugin.h"

#include "UI/Repair/Workbench/SpareMods.h"
#include "UI/Repair/Workbench/Spares.h"

#include <cstdint>
#include <vector>

// The 2 boxes of iKeepSpareMods that let the player pick a spare's mods, at
// Choose mods to use up and Choose mods to keep. Both are headed by the
// vanilla REPAIR and list USE UP THE REST, each mod of the spare that comes
// off and is not picked yet, KEEP THE REST and CANCEL. A pick asks again,
// and nothing is used before the box ends, so CANCEL drops the picks and
// goes back to the box of spares. Private to this folder.
namespace Workbench
{
	// The box for the player's copy of a_object on the stack with a_item and
	// the spare on the stack with a_spare, a_picked the form IDs of the mods
	// picked so far. At Choose mods to use up a pick uses up that mod, and
	// at Choose mods to keep it keeps it. A mod's row ends with the step its
	// pick makes, see iSpareModRows. Both copies are looked up again each
	// time it opens.
	void AskMods(RE::TESBoundObject& a_object, const RE::ExtraDataList* a_item, const RE::ExtraDataList* a_spare,
		const Priced& a_priced, std::vector<std::uint32_t> a_picked, KeepMods a_setting);
}
