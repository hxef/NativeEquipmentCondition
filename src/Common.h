#pragma once

// Structs
struct ConditionObject {
	RE::BGSKeyword* keyword;
	RE::BGSMod::Attachment::Mod* omod;
	RE::BGSConstructibleObject* cobj;
};

// Consts
extern const float MAX_HEALTH;
extern const float INVALID_HEALTH;

// Globals
extern RE::TESDataHandler* g_dataHandler;
extern RE::BGSKeyword* g_apCndKeyword;

extern std::vector<ConditionObject> g_cndObjects;
extern const std::uint8_t NUM_CONDITION_LEVELS;
