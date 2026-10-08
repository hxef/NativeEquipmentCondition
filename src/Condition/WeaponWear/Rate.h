#pragma once

#include "Core/Plugin.h"

// How fast a weapon wears, against the ordinary weapon of the load order.
// Private to this folder.
namespace WeaponWear
{
	// Measures what an ordinary weapon in this load order hits for and how
	// fast an ordinary automatic fires, and says so in the log. a_again for a
	// measure after the first, which logs only what moved.
	void MeasureReference(bool a_again);

	// How much health one shot or swing costs, from the damage this copy deals,
	// what it is built of and, for an automatic, how fast it fires. The weapon
	// and its extra data say what it is built from, the instance data what the
	// copy in hand does.
	float Rate(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData& a_instance,
		const RE::ExtraDataList* a_extra);

	// Prints every place a weapon keeps a damage number, once per weapon form.
	// There are 3: physical damage, a single field. Damage types, a list of
	// forms and values, where a laser keeps its energy damage. And an object
	// effect, a spell the magic code casts on the target. Each meets the
	// condition penalty at a hook of its own, see HealthDamage.h.
	void LogDamageSources(const RE::TESBoundObject& a_object, RE::TBO_InstanceData* a_data, bool a_perStack);
}
