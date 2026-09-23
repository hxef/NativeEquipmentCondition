#pragma once

// The data handler, which the plugin reads the load order through. Set in
// main.cpp once game data has loaded. It outlives a full reset. The player does
// not, so the player is asked for with PlayerCharacter::GetSingleton every
// time.
extern RE::TESDataHandler* g_dataHandler;
