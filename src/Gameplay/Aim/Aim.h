#pragma once

#include "Core/Plugin.h"

// Worn guns are less accurate and carry their damage less far, in anybody's
// hands.
//
// Accuracy. Every shot that spreads, from the hip or down the sights, asks
// AimModel::GetMinMaxCone for its cone once, through ApplyConeOfFire inside
// the engine's Fire. InstallAccuracy hooks that call and widens both cones by
// 1 over the gun's share, a straight line from fAccuracyFloor at 0 to 1 at
// fWeaponPenaltyStart, so at 0.75 a cone is a third wider. Sneaking, sights,
// crippled arms, barrels and the cone perks are already inside the game's
// answer. No cone grows past the game's own caps, read from the loaded game,
// and a cone of 0 or less, an NPC aiming true, stays as it is. The shot's
// condition comes from WeaponEvents' fire call, see Fired, so a shot the game
// fires without that call, a fire queued as a task or a throw, keeps the
// game's spread, and so does an NPC's shot while its inventory is busy.
//
// The crosshair asks the same function for its min and max and for its size
// right now, and both calls widen by the share measured once a frame, see
// InHand.h. It shows the spread, so it widens only while the spread runs.
// VATS never spreads a shot, but its hit chance counts the gun's accuracy as
// 1 term, which is multiplied by the share, for guns only, whoever has the
// spread.
//
// Range. A ranged hit keeps its full damage out to the gun's min range, then
// falls in a straight line to the gun's out of range share at its max range.
// Both ranges are multiplied by what the range perks answer, and InstallRange
// multiplies that answer by the gun's share, a straight line from fRangeFloor
// at 0 to 1 at fWeaponPenaltyStart, so both ranges come in. Up close nothing
// changes. A melee blow and an explosion have no such falloff. VATS reads the
// max range straight off the gun and keeps it.
//
// The item cards show accuracy and range at the same shares, and a container's
// sort uses the same accuracy, see AccuracyShare, RangeShare and
// UI/Inventory/ItemCard/Aim.cpp.
// Combat AI keeps the gun's own numbers.
//
// Accuracy.cpp is accuracy, Range.cpp is range.
namespace Aim
{
	// Patches the spread, the crosshair pair and VATS.
	void InstallAccuracy();

	// Finds the game's caps on the cone, fAimModelFireConeAngleLimitMax and
	// Min. The settings are built into the game, so they live through a full
	// reset, but a plugin can change their values, which are read on every
	// call.
	void LoadAccuracy();

	// Patches the range perk call inside the falloff.
	void InstallRange();

	// Whether the spread runs, and the range patch: not when another mod has
	// the spread, the fire call it reads its shots from, or the falloff. The
	// menus ask once before patching their own calls.
	[[nodiscard]] bool Spreads();
	[[nodiscard]] bool Shortens();

	// Whether a worn gun's shots spread wider right now: the spread runs and
	// fAccuracyFloor is below 1. At 1 every hook gives the game's own
	// spread. WeaponEvents asks it before it reads an NPC's gun.
	[[nodiscard]] bool SpreadsNow();

	// The share of its accuracy, or of its range, a copy of a gun keeps at
	// a_health, for the crosshair and the menus. 1 for anything but a gun
	// and a health below 0, and while the spread, or the falloff, does not
	// run or its floor is 1.
	[[nodiscard]] float AccuracyShare(const RE::TESObjectWEAP& a_weapon, float a_health);
	[[nodiscard]] float RangeShare(const RE::TESObjectWEAP& a_weapon, float a_health);

	// The gun a shot is fired from and its health, while the engine's Fire
	// runs on this thread. WeaponEvents' fire call holds one around Fire for a
	// gun that wears, and the spread reads it. One made inside another hides
	// it until it ends.
	class Fired
	{
	public:
		Fired(const RE::TESObjectREFR* a_shooter, const RE::TESObjectWEAP& a_weapon, float a_health);
		~Fired();
		Fired(const Fired&) = delete;
		Fired& operator=(const Fired&) = delete;

		const RE::TESObjectREFR* shooter;
		const RE::TESObjectWEAP* weapon;
		float                    health;

	private:
		const Fired* outer;
	};
}
