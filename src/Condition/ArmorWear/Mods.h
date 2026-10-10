#pragma once

#include "Core/Plugin.h"

#include <optional>

// Which slots of a piece take a mod that adds protection, an effect or a
// bonus, measured by LoadMods and MeasureModsAgain, see ArmorWear.h. Private to
// this folder.
namespace ArmorWear
{
	// Whether a mod that adds something fits a_armor in one of its slots. The
	// piece's slots and keywords are read as they are now. Empty while no
	// table is built: before LoadMods, after UnloadMods, and when the armor
	// mods could not be read.
	std::optional<bool> GivingModFits(const RE::TESObjectARMO& a_armor);

	// Whether a table is built, see GivingModFits.
	bool ModsMeasured();
}
