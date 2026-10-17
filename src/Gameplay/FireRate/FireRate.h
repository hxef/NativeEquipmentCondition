#pragma once

#include "Core/Plugin.h"

#include <optional>

namespace FireRate
{
	// A worn automatic weapon fires slower.
	//
	// An automatic gun fires a held burst from its attack animation, which
	// loops at the gun's speed, so an automatic receiver or the Rapid legendary
	// makes it faster. Install hooks the one call the animation reads an
	// actor's weapon speed through and returns a share of it, a straight line
	// from fFireRateFloor at 0 to 1 at fWeaponPenaltyStart. 2 more places get
	// the same share: the countdown between 2 presses of the trigger, and the
	// automatic weapon sound, which picks the loop nearest to the rate. The
	// menus show and compare the rate at the same share, see RateShare. Combat
	// AI and VATS keep the gun's own numbers.
	//
	// The player's share comes from the condition measured once a frame, see
	// InHand.h, since the animation asks from wherever it updates. An NPC
	// fires at its weapon's condition the same way, its share read as it uses
	// the weapon, see NoteNpcWeapon. Combat AI times a burst in seconds, so a
	// slower gun fires fewer shots per burst on its own.
	//
	// Automatic weapons only, since on anything else the trigger finger sets
	// the pace. That includes the Ripper and the Mr. Handy Buzz Blade, whose
	// blades run on a motor. No melee animation reads the weapon speed, and a
	// blade's held attack cuts at the HitFrame events of its clip, 5 a second.
	// So Install also hooks the call that queues each cut and lets through only
	// a worn blade's share of them, 3 in 4 at 0.75. Its bash and its power
	// attack land in full. The minigun's Shredder bash is held too. Its 3rd
	// person clip plays at the weapon speed, and its first person one does not,
	// so the player's cuts in first person are paced the same way.
	void Install();

	// Forgets every NPC's share and every blade's pace. Form IDs name
	// something else after a full reset, and other guns and NPCs once another
	// game starts, so it runs then too, see ForgetPlay in main.cpp.
	void Unload();

	// Whether the fire speed, countdown and blade cut places all run: not
	// once another mod has one of them, at install or since. The menus ask
	// once before patching their own fire rate calls.
	[[nodiscard]] bool Slows();

	// Whether a worn automatic weapon fires slower right now: the pace runs
	// and fFireRateFloor is below 1. At 1 every hook gives the game's own
	// rate. WeaponEvents asks it before it reads an NPC's weapon.
	[[nodiscard]] bool SlowsNow();

	// The share of its own rate a copy of a weapon fires at in this condition,
	// for the menus, see ItemCard/Rate.cpp. 1 for a weapon that is not
	// automatic or carries no condition, and while SlowsNow is false.
	[[nodiscard]] float RateShare(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data, float a_health);

	// Reads the condition of the weapon an NPC just used, for the pace its
	// animation and sound keep from then on. WeaponEvents calls it for every
	// NPC blow, which counts for a blade like the Ripper, and the overload
	// below for every NPC shot. Anything but an automatic weapon that wears
	// is passed over. The lock is taken as a writer and never waited for, see
	// Equipped::TryWeaponHealth, so a busy inventory keeps the last share.
	void NoteNpcWeapon(RE::Actor& a_actor, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon);

	// The same with the health the caller read through TryWeaponHealth,
	// nothing while the inventory was busy, so 1 reading serves the fire hook.
	void NoteNpcWeapon(RE::Actor& a_actor, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, std::optional<float> a_health);
}
