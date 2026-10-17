#pragma once

#include "Core/Plugin.h"

// Which weapons take part, how fast one wears, and wearing down the copy in
// hand. Every shot and hit costs the equipped copy some condition, based on how
// hard it hits compared with the ordinary weapon of the load order and on what
// it is built of, see Materials.h. Rate.h has the rate, the ordinary weapon and
// the ordinary automatic, Copy.h the write the player's wear and an NPC's
// share, and Theirs.cpp an NPC's wear.
namespace WeaponWear
{
	// Measures the ordinary weapon and the ordinary automatic of this load
	// order. Keeps numbers and no forms, so there is nothing to unload.
	void Load();

	// Measures the ordinary weapon and the ordinary automatic again, at every
	// save load, new game and coc from the main menu. A DLL mod such as RobCo
	// Patcher can change weapons after Load measured, as game data loads.
	// Says only what moved.
	void MeasureAgain();

	// What that ordinary weapon hits for, the number every wear rate is
	// measured against, a weapon's per shot and a piece of armor's per hit.
	float ReferenceDamage();

	// Whether this weapon takes part, as a reason: nullptr for one that does,
	// otherwise what ruled it out. It must be playable, since kNotPlayable
	// marks turret guns, robot guns and creature attacks. And it must not be
	// thrown: grenades and mines are used up in one go and stack, so a
	// condition each would split a stack of 20 into 20 entries, and they sit in
	// a second equip slot beside the drawn weapon, which would confuse
	// Equipped::WeaponHealth.
	const char* WhyNoCondition(const RE::TESObjectWEAP& a_weapon);

	// Wears down the player's equipped copy of a weapon, and that copy alone,
	// see Equipped::WriteStack.
	//
	// a_scale multiplies what this one event costs: above 1 for a bash and a
	// power attack, and for a shot the power its rounds carried, below 1 for a
	// charge not held to full. Returns true when the health changed. Call it
	// without holding the inventory lock, see ItemCards.h.
	bool Wear(RE::TESObjectREFR& a_owner, RE::TESObjectWEAP& a_weapon, const char* a_source, float a_scale = 1.0F);

	// Whether NPC weapons wear at all, false at an fNpcWearRateMult of 0 or
	// below, so the hooks pass their shots and blows by.
	bool TheirsWear();

	// Wear for an NPC's copy, at fNpcWearRateMult with its lines in the NPC
	// log. It never waits for their inventory: while it is busy the wear
	// waits and is tried again at the next shot or blow of anybody, see
	// Theirs.cpp. It is dropped only when they are gone or no longer hold the
	// weapon, after many busy tries, or when the game ends with a save load, a
	// new game, the main menu or a full reset. An essential NPC's own weapon
	// from the editor keeps its condition, see Equipped::Issued, and a weapon
	// worn to 0 stays in their hands: the game only refuses to equip it again.
	void WearTheirs(RE::Actor& a_owner, RE::TESObjectWEAP& a_weapon, const char* a_source, float a_scale);

	// Queues a try of the NPC wear still waiting, if any waits and no try is
	// queued. 1 relaxed atomic load when nothing waits, so every shot and hit
	// calls it. Call it holding no inventory lock.
	void SettleSoon();

	// Drops the NPC wear still waiting for a busy inventory, since it belongs
	// to the last game. Also run at a full reset, when the weapons it names
	// are deleted.
	void ForgetOwed();
}
