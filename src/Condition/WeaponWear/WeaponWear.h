#pragma once

#include "Core/Plugin.h"

// Which weapons take part, how fast one wears, and wearing down the copy in
// hand. Every shot and hit costs the equipped copy some condition, based on how
// hard it hits compared with the ordinary weapon of the load order and on what
// it is built of, see Materials.h. Rate.h has the rate, the ordinary weapon
// and the ordinary automatic.
namespace WeaponWear
{
	// Measures the ordinary weapon and the ordinary automatic of this load
	// order. Keeps numbers and no forms, so there is nothing to unload.
	void Load();

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

	// Wears down the equipped copy of a weapon, and that copy alone, see
	// Equipped::WriteEquipped.
	//
	// a_scale multiplies what this one event costs: above 1 for a bash and a
	// power attack, and for a shot the power its rounds carried, below 1 for a
	// charge not held to full. Returns true when the health changed, which is
	// when the caller refreshes the Pip-Boy. Call it without holding the
	// inventory lock, see ItemCards.h.
	bool Wear(RE::TESObjectREFR& a_owner, RE::TESObjectWEAP& a_weapon, const char* a_source, float a_scale = 1.0F);
}
