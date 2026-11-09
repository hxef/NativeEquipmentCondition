#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace HealthDamage
{
	// Scales weapon damage by the item's health extra data. The engine looks up
	// the equipped item's health on every attack and hands it to
	// CombatFormulas::CalcWeaponDamage, and the Pip-Boy reaches the same
	// function through GetWeaponDisplayDamage. It never uses the number:
	// Skyrim's tempering curve only adds damage and is not used for guns.
	// Install hooks every call to CalcWeaponDamage and scales the result, which
	// also covers the Pip-Boy.
	//
	// A weapon's object effects are spells, the radiation a Radium Rifle
	// applies to what it hits, and the engine casts them at full power. Install
	// multiplies their power by the share of damage the condition leaves, see
	// Curve.h, so they weaken with wear like the rest.
	//
	// Everybody fights at the condition their weapon is in, so a raider's
	// rusted pipe gun does less damage than a new one. NPC weapons wear at
	// fNpcWearRateMult, see WeaponEvents.h.
	//
	// Combat.cpp is the blow, Effect.cpp a weapon's object effects, Blast.cpp
	// an explosion and Card.cpp the item card. Curve.h is the damage line,
	// Trace.h the trace lines, and Hooks.h what they share.
	void Install();

	// The damage the item card prints for the weapon on a_stack as if it were
	// at a_health, 1 pair per damage type in the card's order: the type as the
	// card numbers it, and the damage before the card drops its fraction. It
	// runs the card's own sum with the card hooks reading a_health, so a
	// weapon on a list in NEC/Exclude, or a card whose hooks are out, prints
	// the same at every health.
	[[nodiscard]] std::vector<std::pair<std::uint32_t, float>> CardDamage(const RE::BGSInventoryItem& a_item,
		const RE::BGSInventoryItem::Stack* a_stack, float a_health);

	// The share of its damage a weapon at a_health keeps, the line every hook
	// above applies, see Curve.h.
	[[nodiscard]] float DamageShare(float a_health);
}
