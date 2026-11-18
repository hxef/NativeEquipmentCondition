#pragma once

#include "UI/Roles/Roles.h"

#include <string_view>

// The list columns a menu movie lets mods add: a class of the movie's own,
// named in Conventions.h, whose settings hold a comma list of item card rows
// that each get a column in the movie's item lists. Found by the class and
// its members, never by a mod's name.
namespace Roles
{
	// Whether a_movie's lists take column keys from its own settings. While
	// a_give, a_row joins those keys once the settings have loaded, so the
	// movie draws and sizes that row's column itself, and sorts by it where
	// its lists sort. A row already in the keys is not added again. The keys
	// live in the movie's memory only and go with it, so nothing is taken
	// back and no file is written. True whenever the keys were found, with
	// a_give off too, so the caller learns the shape either way.
	[[nodiscard]] bool GiveColumn(Scaleform::GFx::Movie& a_movie, std::string_view a_row, bool a_give);
}
