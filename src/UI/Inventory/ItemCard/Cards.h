#pragma once

#include "Core/Plugin.h"

#include <string_view>

// What the files of this feature share. Private to this folder.
namespace ItemCard
{
	// The game's own translation key for condition, CND in English, so the row
	// reads right in every language.
	inline constexpr const char* CND_TEXT = "$ItemInfo_CND";

	// Patches the calls that fill item cards, and while FireRate slows worn
	// weapons the fire rate calls too. Returns whether either CND call was
	// patched.
	bool PatchCards();

	// Adds the render listener that keeps a card's CND row above Damage to
	// a menu movie that draws item cards, and passes over any other movie.
	void WatchCard(Scaleform::GFx::Movie& a_movie, std::string_view a_file);
}
