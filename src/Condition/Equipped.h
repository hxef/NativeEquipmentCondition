#pragma once

#include "Core/Plugin.h"

#include <optional>

// Reading the condition of what an actor holds. Every reader walks the actor's
// inventory for the equipped copy of a weapon that takes part, under the
// inventory read lock.
//
// TryWeaponHealth takes the lock as a writer instead, for a caller that is not
// running where the engine's own reader does. Other game threads split, merge
// and free stacks while holding only the read lock, so a walk could land on a
// stack as it is freed. It never waits for the lock: F4SE holds its queue while
// a task runs, so a thread holding the lock and queueing a task would wait on
// this one and the game would freeze. A failed try keeps the caller's last
// value.
namespace Equipped
{
	// The weapon the actor holds, or nothing when none takes part. Needed only
	// for the one bash path that names no weapon, see WeaponEvents/Hits.cpp.
	// WearsOut keeps grenades and mines out, so a rifle wins over the grenades
	// carried beside it.
	RE::TESObjectWEAP* Weapon(RE::Actor* a_actor);

	// The health of the equipped weapon, or INVALID_HEALTH when the actor holds
	// none that takes part. a_weapon limits the search to that form, so a
	// grenade, a mine or a shot fired before a weapon swap does not take the
	// condition of whatever is in hand when it lands. a_effect limits it to the
	// copy that casts that object effect, since the cast that applies a shot's
	// effect to the target names no weapon.
	float WeaponHealth(RE::Actor* a_actor, const RE::TESForm* a_weapon = nullptr,
		const RE::EnchantmentItem* a_effect = nullptr);

	// WeaponHealth for a caller outside the engine's own code path, a task on
	// F4SE's queue or an NPC's shot or hit. Nothing while the inventory is
	// busy.
	std::optional<float> TryWeaponHealth(RE::Actor* a_actor, const RE::TESForm* a_weapon = nullptr);
}
