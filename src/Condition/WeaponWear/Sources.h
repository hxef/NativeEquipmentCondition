#pragma once

#include "Core/Plugin.h"

// The trace log's list of where each weapon keeps its damage.
// Private to this folder.
namespace WeaponWear
{
	// Prints every place a weapon keeps a damage number, once per weapon form.
	// There are 3: physical damage, a single field. Damage types, a list of
	// forms and values, where a laser keeps its energy damage. And an object
	// effect, a spell the magic code casts on the target. Each meets the
	// condition penalty at a hook of its own, see HealthDamage.h.
	void LogDamageSources(const RE::TESBoundObject& a_object, RE::TBO_InstanceData* a_data, bool a_perStack);

	// Forgets which weapons were listed, so each is listed again.
	void ForgetSources();
}
