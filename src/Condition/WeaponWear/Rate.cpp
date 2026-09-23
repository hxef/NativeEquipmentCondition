#include "Condition/WeaponWear/Rate.h"

#include "Condition/Condition.h"
#include "Condition/Materials.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <cstdint>
#include <mutex>

namespace WeaponWear
{
	namespace
	{
		// Health the reference weapon loses per shot or swing, before
		// fWearRateMult. About 1100 uses from new to broken, long enough that
		// condition is something to keep an eye on. An fWearRateMult of 60 cuts
		// that to under 20, which is how the wear is tested.
		constexpr float RATE_AT_REFERENCE = 0.0009F;

		// What the reference weapon hits for. Measured by Load, since an
		// overhaul that multiplies every damage in the game would otherwise
		// wear everything out many times faster. Vanilla's own value is used
		// until then.
		float g_referenceDamage = 30.0F;

		// What the round does where it lands. A Fat Man's record reads 18
		// damage and a Broadsider's 33, since the damage belongs to the shell,
		// and counting the tube alone had a Fat Man lasting 1500 mini nukes.
		// The engine adds the same number for the item card, see
		// CombatFormulas::GetWeaponDisplayDamage. Its own projectile lookup is
		// used, so a barrel that swaps the projectile and a change of
		// ammunition are both followed.
		float ExplosionDamage(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData& a_instance)
		{
			const auto* projectile = RE::TESObjectWEAP::GetProjectile(&a_weapon, a_instance.ammo, &a_instance);
			const auto  bursts = static_cast<std::uint32_t>(RE::BGSProjectile::BGSProjectileFlags::kExplosion);
			if (!projectile || !(projectile->data.flags & bursts)) {
				return 0;
			}

			// A projectile can be flagged to explode and name no explosion, a
			// record left half finished.
			const auto* explosion = projectile->data.explosionType;
			return explosion ? std::max(explosion->data.damage, 0.0F) : 0.0F;
		}

		// What the gun itself hits for, read the way the engine reads it. A
		// round's own damage is added to the weapon's, so ammunition that hits
		// harder wears the gun faster. Every vanilla round carries 0. A gun
		// with no ammunition reads secondaryDamage, the engine's own fallback.
		float BaseDamage(const RE::TESObjectWEAP::InstanceData& a_instance)
		{
			if (a_instance.ammo) {
				return static_cast<float>(a_instance.attackDamage) + a_instance.ammo->data.damage;
			}
			if (a_instance.type == RE::WEAPON_TYPE::kGun) {
				return a_instance.secondaryDamage;
			}
			return static_cast<float>(a_instance.attackDamage);
		}

		// Every kind of damage, not the physical alone. A laser keeps all of
		// its damage in damage types and reads 0 physical.
		float TotalDamage(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData& a_instance)
		{
			auto damage = BaseDamage(a_instance);
			if (a_instance.damageTypes) {
				for (const auto& entry : *a_instance.damageTypes) {
					// Whole numbers, which is how the engine reads them too.
					damage += static_cast<float>(entry.second.i);
				}
			}
			return damage + ExplosionDamage(a_weapon, a_instance);
		}

		// Prints the magic effects behind a spell a weapon casts on what it
		// hits, its object effect or its critical effect. The magnitude is what
		// a weapon in full condition applies to the target.
		void LogEffects(const char* a_what, const RE::MagicItem* a_item)
		{
			if (!a_item) {
				return;
			}

			TraceLog::Line("sources", "  {:s} {:s} [{:08X}], {:d} effects",
				a_what, RE::TESFullName::GetFullName(*a_item), a_item->formID, a_item->listOfEffects.size());

			for (const auto* effect : a_item->listOfEffects) {
				if (!effect) {
					continue;
				}
				const auto* setting = effect->effectSetting;
				TraceLog::Line("sources", "    {:s} [{:08X}] magnitude {:.2f}",
					setting ? RE::TESFullName::GetFullName(*setting) : "?"sv,
					setting ? setting->formID : 0, effect->data.magnitude);
			}
		}
	}

	void MeasureReference()
	{
		// The median weapon, not the average. A few hit for hundreds while most
		// hit for tens.
		std::vector<float> damages;
		for (auto* weapon : g_dataHandler->GetFormArray<RE::TESObjectWEAP>()) {
			if (!weapon || !Condition::WearsOut(*weapon)) {
				continue;
			}
			const auto damage = TotalDamage(*weapon, weapon->weaponData);
			if (damage > 0.0F) {
				damages.push_back(damage);
			}
		}

		if (!damages.empty()) {
			const auto middle = damages.begin() + damages.size() / 2;
			std::nth_element(damages.begin(), middle, damages.end());
			g_referenceDamage = *middle;
		}

		REX::INFO("An ordinary weapon in this load order hits for {:.0f}.", g_referenceDamage);
	}

	void LogDamageSources(const RE::TESBoundObject& a_object, RE::TBO_InstanceData* a_data, bool a_perStack)
	{
		if (!a_object.IsWeapon() || !a_data) {
			return;
		}

		// Several threads wear weapons, so the set of forms already reported
		// needs a lock. Touched once per weapon.
		static std::mutex                       seenLock;
		static std::unordered_set<std::uint32_t> seen;
		{
			const std::scoped_lock l(seenLock);
			if (!seen.insert(a_object.formID).second) {
				return;
			}
		}

		const auto* weapon = static_cast<const RE::TESObjectWEAP::InstanceData*>(a_data);

		TraceLog::Line("sources", "{:s} [{:08X}] from {:s} instance data, physical damage {:d}",
			RE::TESFullName::GetFullName(a_object), a_object.formID,
			a_perStack ? "per stack" : "base form", weapon->attackDamage);

		if (weapon->damageTypes && !weapon->damageTypes->empty()) {
			for (const auto& entry : *weapon->damageTypes) {
				const auto* type = entry.first;
				TraceLog::Line("sources", "  damage type {:s} [{:08X}] value {:d}",
					type ? RE::TESFullName::GetFullName(*type) : "?"sv,
					type ? type->formID : 0, entry.second.i);
			}
		} else {
			TraceLog::Line("sources", "  no damage types");
		}

		// The weapon's own object effect comes off the form, the ones its mods
		// add off this copy's data.
		LogEffects("object effect", static_cast<const RE::TESObjectWEAP&>(a_object).GetBaseEnchanting());
		if (weapon->enchantments) {
			for (const auto* effect : *weapon->enchantments) {
				LogEffects("mod object effect", effect);
			}
		}
		LogEffects("critical effect", weapon->effect);
	}

	float Rate(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData& a_instance,
		const RE::ExtraDataList* a_extra)
	{
		const auto damage = TotalDamage(a_weapon, a_instance);
		const auto blast = ExplosionDamage(a_weapon, a_instance);

		// What the weapon is built from, see Materials.h. This and both
		// references are read from the game as it is loaded.
		const auto quality = Materials::Quality(a_weapon, a_extra);

		// Harder hitting wears faster, better built wears slower. A pipe gun is
		// steel and hits for little, a plasma rifle is nuclear material and
		// hits hard, and both land near the middle. A pipe rifle rechambered to
		// .50 wears out fast, and a well made gun with low damage lasts. Both
		// are measured against the reference weapon, so how much damage a
		// weapon can deal in its life depends on what it is built from, and
		// damage per shot decides how fast that runs out.
		// fWearRateMult stops at 0, since below it a gun would gain condition
		// as it fires.
		const auto mult = Settings::fWearRateMult.GetValue();
		const auto wear = RATE_AT_REFERENCE * (mult > 0.0F ? mult : 0.0F) *
		                  (damage / g_referenceDamage) * (Materials::ReferenceQuality() / quality);

		if (blast > 0.0F) {
			TraceLog::Line("rate", "{:.0f} damage, {:.0f} of it the blast, at quality {:.1f} costs {:.6f} a shot",
				damage, blast, quality, wear);
		} else {
			TraceLog::Line("rate", "{:.0f} damage at quality {:.1f} costs {:.6f} a shot", damage, quality, wear);
		}
		return wear;
	}
}
