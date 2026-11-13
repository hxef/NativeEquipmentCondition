#pragma once

#include "Core/Plugin.h"

// How fast a weapon wears, against the ordinary weapon of its damage type in
// the load order.
// Private to this folder.
namespace WeaponWear
{
	// Measures what an ordinary weapon in this load order hits for, for each
	// damage type that enough weapons deal mostly, and how fast an ordinary
	// automatic fires, and says so in the log. a_again for a measure after the
	// first, which logs only what moved.
	void MeasureReference(bool a_again);

	// fNpcWearRateMult, 0 for anything below it.
	float NpcMult();

	// fAutoWearRateMult, kept between 0 and 10.
	float AutoMult();

	// fSuppressedWearRateMult, kept between 0.5 and 2.
	float SuppressedMult();

	// HasSilencer, the keyword every vanilla suppressor adds to the copy,
	// looked up at each call, so nothing is kept to unload. Null when no
	// plugin loaded has it.
	const RE::BGSKeyword* SuppressorKeyword();

	// How much health one shot or swing costs, from the damage this copy deals,
	// what it is built of and, for an automatic, how fast it fires. The weapon
	// and its extra data say what it is built from, the instance data what the
	// copy in hand does. a_theirs for an NPC's copy, at fNpcWearRateMult with
	// the line in the NPC log. a_bash for a bash, which fAutoWearRateMult and
	// fSuppressedWearRateMult leave alone.
	float Rate(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData& a_instance,
		const RE::ExtraDataList* a_extra, bool a_theirs, bool a_bash);
}
