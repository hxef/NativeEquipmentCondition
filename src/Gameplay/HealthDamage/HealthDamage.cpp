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
		REX::INFO("Everybody fights at the condition their weapon is in, and only the player's weapons wear.");
	}
}
