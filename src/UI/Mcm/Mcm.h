#pragma once

#include "Core/Plugin.h"

#include <string_view>

// The settings page in the Mod Configuration Menu. MCM builds its pages in the
// pause menu's movie from Data\MCM\Config\NEC\config.json and asks its code
// object, root.mcm, for every value, every change and, for an entry marked
// textFromFormName, every line of text. NEC puts its own functions on root.mcm
// in front of MCM's, answers what is NEC's from Settings, Core/Text and the
// HUD's measure, and passes the rest on. Each switch sits in a block with
// its sliders. A part left to another mod shows as a grey line at the top of
// its block, see Notes.cpp, and in a list at the top of NEC's main page, see
// TopList.cpp. The main page also picks, saves, resets and deletes presets,
// see PresetRows.cpp and PresetButtons.cpp.
//
// A change applies on the spot. Once it has stood for half a second it is
// written into NEC_custom.ini, or into the picked preset's file, see
// Core/Presets/Presets.h, on the next frame of the main or pause menu, so a
// change the game quits or crashes before is lost from the file.
namespace Mcm
{
	// Hooks MainMenu.swf, which the main and pause menus share, and ignores
	// every other movie.
	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file);

	// Puts the functions the Reset preset and Delete preset buttons call on
	// a_plugin, the object F4SE gives NEC in MainMenu.swf, in
	// PresetButtons.cpp.
	void AddPresetButtons(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_plugin);
}
