#include "Gameplay/Aim/Aim.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <cstdint>

namespace Aim
{
	namespace
	{
		// The 1 call to BGSEntryPoint::HandleEntryPoint inside
		// CombatFormulas::CalcRangeDamageMult, which asks the range perks
		// (kModGunRangeMult) for any shooter and multiplies both of the gun's
		// ranges by their answer. HitData::Populate works the falloff out for
		// every blow that is not melee, and Projectile::GetRangeMult for every
		// round that hits.
		constexpr CallPatch::CallSite RANGE_SITE{ RE::ID::CombatFormulas::CalcRangeDamageMult.id(), 0x71, "range falloff" };

		// HandleEntryPoint takes as many arguments as its entry point needs,
		// here the shooter, the gun and the multiplier the perks change.
		using Perks_t = void (*)(RE::BGSEntryPoint::ENTRY_POINT, RE::Actor*, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>*, float*);

		CallPatch::Link<Perks_t> g_rangeLink;

		// fRangeFloor, kept between 0 and 1. A value that is not a number
		// counts as 0. At 0 a broken gun hits at its out of range share at any
		// distance.
		float Floor()
		{
			const auto floor = Settings::fRangeFloor.GetValue();
			return floor >= 0.0F ? std::min(floor, 1.0F) : 0.0F;
		}

		float Share(float a_health)
		{
			return Condition::Share(a_health, Floor(), Condition::Kind::kWeapon);
		}

		// Whether wear shortens range at all.
		bool Lowers()
		{
			return Floor() < 1.0F;
		}

		// Stands in for HandleEntryPoint where the falloff asks the range
		// perks. They answer first, so a perk that sets the multiplier rather
		// than multiply it is still scaled after.
		void RangePerksHk(RE::BGSEntryPoint::ENTRY_POINT a_entryPoint, RE::Actor* a_shooter,
			const RE::BGSObjectInstanceT<RE::TESObjectWEAP>* a_weapon, float* a_mult)
		{
			g_rangeLink(a_entryPoint, a_shooter, a_weapon, a_mult);

			const auto* object = a_weapon ? a_weapon->object : nullptr;
			if (!g_rangeLink.Live() || !Lowers() || !a_shooter || !a_mult || !object || !object->IsWeapon()) {
				return;
			}
			const auto& weapon = static_cast<const RE::TESObjectWEAP&>(*object);
			if (!weapon.IsGunWeapon() || !Condition::WearsOut(weapon)) {
				return;
			}

			// Inside a hit, read as the damage type hook in
			// HealthDamage/Combat.cpp reads it.
			const auto health = Equipped::WeaponHealth(a_shooter, &weapon);
			const auto share = Share(health);
			const auto perks = *a_mult;
			*a_mult = perks * share;

			if (!TraceLog::IsOpen()) {
				return;
			}
			const auto* data = static_cast<const RE::TESObjectWEAP::InstanceData*>(a_weapon->instanceData.get());
			const auto& stats = data ? *data : weapon.weaponData;
			if (a_shooter == RE::PlayerCharacter::GetSingleton()) {
				TraceLog::Line("range", "{:s}  health {:.6f}  share {:.4f}  falloff from {:.0f} to {:.0f}, new {:.0f} to {:.0f}",
					TraceLog::Who{ &weapon }, health, share, stats.minRange * *a_mult, stats.maxRange * *a_mult, stats.minRange * perks,
					stats.maxRange * perks);
			} else {
				TraceLog::Npc::Line("range", "{:s} with {:s}  health {:.6f}  share {:.4f}  falloff from {:.0f} to {:.0f}, new {:.0f} to {:.0f}",
					TraceLog::Who{ a_shooter }, TraceLog::Who{ &weapon }, health, share, stats.minRange * *a_mult, stats.maxRange * *a_mult,
					stats.minRange * perks, stats.maxRange * perks);
			}
		}
	}

	void InstallRange()
	{
		if (!CallPatch::PatchCall(RANGE_SITE, RE::ID::BGSEntryPoint::HandleEntryPoint.address(),
				reinterpret_cast<std::uintptr_t>(&RangePerksHk), g_rangeLink)) {
			REX::WARN("A worn gun will keep the range of a new one.");
			return;
		}
		REX::INFO("A worn gun loses damage sooner at range, for anyone, its ranges down to {:.2f} at 0 condition.", Floor());
	}

	bool Shortens()
	{
		return g_rangeLink.Live();
	}

	float RangeShare(const RE::TESObjectWEAP& a_weapon, float a_health)
	{
		return Shortens() && Lowers() && a_weapon.IsGunWeapon() ? Share(a_health) : 1.0F;
	}
}
