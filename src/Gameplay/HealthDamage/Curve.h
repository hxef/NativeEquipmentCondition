#pragma once

#include "Core/Plugin.h"

#include "Condition/Condition.h"
#include "Condition/WearList/WearList.h"
#include "Core/Settings.h"

#include <algorithm>
#include <optional>

// The damage line every hook of this feature applies. Private to this folder.
namespace HealthDamage
{
	// What a file in NEC/Wear says a copy of a weapon keeps of its damage at
	// 0 condition, and which line says so, or nothing for no such line. Found
	// with the copy's health, see Equipped::WeaponHealth and Card.cpp.
	using Floor = std::optional<WearList::Set>;

	// Fallout 3 turned condition into damage with a straight line,
	// fDamageGunWeapCondBase + fDamageGunWeapCondMult times condition, shipped
	// as 0.66 and 0.34, so 0 condition hits for 66%. New Vegas kept the shape.
	// The floor is fDamageFloor in NEC.ini. A floor of 0.05 makes the penalty
	// impossible to miss, which is how it is tested. It is kept between 0 and
	// 1, since above 1 a worn gun would do more damage than a new one.
	inline float DamageFloor()
	{
		const auto floor = Settings::fDamageFloor.GetValue();
		return floor > 0.0F ? std::min(floor, 1.0F) : 0.0F;
	}

	// For Papyrus GetDamageShare, which names no weapon.
	inline float DamageMult(float a_health)
	{
		return Condition::Share(a_health, DamageFloor(), Condition::Kind::kWeapon);
	}

	// a_floor's share, else DamageFloor read live. The wear list reads
	// only shares from 0 to 1.
	inline float FloorOr(const Floor& a_floor)
	{
		return a_floor ? a_floor->value : DamageFloor();
	}

	// The share a copy of a weapon at a_health keeps, with its own floor.
	inline float DamageMult(float a_health, const Floor& a_floor)
	{
		return Condition::Share(a_health, FloorOr(a_floor), Condition::Kind::kWeapon);
	}
}
