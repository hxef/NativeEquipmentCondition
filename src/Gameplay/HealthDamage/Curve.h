#pragma once

#include "Core/Plugin.h"

#include "Condition/Condition.h"
#include "Core/Settings.h"

#include <algorithm>

// The damage line every hook of this feature applies. Private to this folder.
namespace HealthDamage
{
	// Fallout 3 turned condition into damage with a straight line,
	// fDamageGunWeapCondBase + fDamageGunWeapCondMult times condition, shipped
	// as 0.66 and 0.34, so 0 condition hits for 66%. New Vegas kept the shape.
	// The floor is fDamageFloor in NEC.ini. A floor of 0.05 makes the penalty
	// impossible to miss, which is how it is tested. It is kept between 0 and
	// 1, since above 1 a worn gun would do more damage than a new one.
	inline float DamageMult(float a_health)
	{
		const auto floor = Settings::fDamageFloor.GetValue();
		return Condition::Share(a_health, floor > 0.0F ? std::min(floor, 1.0F) : 0.0F);
	}
}
