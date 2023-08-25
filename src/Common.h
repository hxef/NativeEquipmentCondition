#pragma once

// Structs
struct ConditionObject {
	RE::BGSKeyword* keyword;
	RE::BGSMod::Attachment::Mod* omod;
	RE::BGSConstructibleObject* cobj;
};

// Globals
extern RE::TESDataHandler* g_dataHandler;
// Attach point keyword for condition mods.
extern RE::BGSKeyword* g_apCndKeyword;
extern RE::PlayerCharacter* g_player;

// List of condition related objects sorted from worst to best.
extern std::vector<ConditionObject> g_cndObjects;
extern const std::uint8_t NUM_CONDITION_LEVELS;
