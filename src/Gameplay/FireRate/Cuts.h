#pragma once

#include "Core/Plugin.h"

// A worn blade's held attack, see FireRate.h, and the shares it reads, which
// FireRate.cpp measures for the animation. Private to this folder.
namespace FireRate
{
	// Patches the call that queues each cut of a held attack, in Cuts.cpp.
	// False when another mod has it.
	[[nodiscard]] bool InstallCuts();

	// Forgets how far each blade is towards its next cut, in Cuts.cpp.
	void ForgetCuts();

	// Whether this copy keeps attacking while the trigger is held: an
	// automatic gun or a motor driven blade. The copy's own data where it
	// has any, since a receiver is what makes most guns automatic.
	bool IsAutomatic(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data);

	// The share for a_weapon, or 1 when the last measurement was for
	// another gun, the case for a frame after a swap.
	float ShareOf(const RE::TESObjectWEAP& a_weapon);

	// The share for the weapon an NPC holds, or 1 when it has not used that
	// weapon since.
	float NpcShareOf(const RE::TESForm& a_actor, const RE::TESObjectWEAP& a_weapon);
}
