#include "Gameplay/HealthDamage/HealthDamage.h"

#include "Gameplay/HealthDamage/Curve.h"
#include "Gameplay/HealthDamage/Hooks.h"

namespace HealthDamage
{
	void Install()
	{
		InstallCombat();
		InstallEffect();
		InstallBlast();
		InstallCard();

		REX::INFO("Condition floor is {:.2f} of full damage.", DamageFloor());
		REX::INFO("Everybody fights at the condition their weapon is in.");
	}

	float DamageShare(float a_health)
	{
		return DamageMult(a_health);
	}

	float DamageShare(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_instance,
		const RE::ExtraDataList* a_extra, float a_health)
	{
		return DamageMult(a_health, WearList::Of(a_weapon, a_instance, a_extra).damage);
	}
}
