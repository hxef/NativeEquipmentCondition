#include "Gameplay/HealthDamage/Hooks.h"

#include "Condition/Equipped.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/TraceLog.h"
#include "Gameplay/HealthDamage/Curve.h"
#include "Gameplay/HealthDamage/Trace.h"

#include <atomic>
#include <cstdint>

namespace HealthDamage
{
	namespace
	{
		using CallPatch::CallSite;

		// The call to MagicCaster::Cast that carries a weapon's object effects.
		// Object effects are spells, the radiation a Radium Rifle applies to
		// what it hits, and a mod on the weapon can add more, a legendary among
		// them. Every cast reaches Cast with a power, the engine's multiplier
		// on the magnitude, and for a weapon it is always full. The site
		// follows the caster's own check on a spell. The engine casts a blow's
		// effects from its task queue, so they can land a moment after the
		// blow.
		constexpr CallSite HIT_EFFECT_SITE{ 2226293, 0x0E4, "hit effect" };

		CallPatch::Link<bool(RE::MagicCaster*, float, std::uint32_t*, RE::TESBoundObject*, bool, bool)> g_hitEffect;

		// -------------------------------------------------------------------
		// The object effect hooks
		// -------------------------------------------------------------------

		// Casts a weapon's object effects at the condition the weapon is in,
		// multiplied into the power the way Combat.cpp multiplies it into the
		// damage types' range multiplier. A blow struck by hand names its
		// weapon as the source. A shot names none, so the effect is asked which
		// weapon in the shooter's hands casts it. A spell, a chem or a critical
		// effect keeps the power it came with.
		bool HitEffectHk(RE::MagicCaster* a_caster, float a_power, std::uint32_t* a_targets, RE::TESBoundObject* a_source,
			bool a_noHitArt, bool a_hostileOnly)
		{
			const auto* spell = a_caster->currentSpell;
			const auto* effect = spell ? spell->As<RE::EnchantmentItem>() : nullptr;
			auto*       actor = effect && g_hitEffect.Live() ? a_caster->GetCasterAsActor() : nullptr;
			if (!actor) {
				return g_hitEffect(a_caster, a_power, a_targets, a_source, a_noHitArt, a_hostileOnly);
			}

			const auto health = Equipped::WeaponHealth(actor, a_source, effect);
			const auto mult = DamageMult(health);

			// Every pellet of one shell casts the same effect again, so a line
			// is written only when the health changes.
			if (actor == RE::PlayerCharacter::GetSingleton()) {
				static std::atomic<std::uint64_t> lastPlayer{ 0 };
				if (!Repeats(lastPlayer, { health, mult }, effect->formID)) {
					TraceLog::Line("effect", "{:s} [{:08X}]  health {:.6f}  x {:.4f}",
						RE::TESFullName::GetFullName(*effect), effect->formID, health, mult);
				}
			} else if (mult != 1.0F) {
				static std::atomic<std::uint64_t> lastNpc{ 0 };
				if (!Repeats(lastNpc, { health, mult }, NpcScope(*actor))) {
					TraceLog::Npc::Line("effect", "{:s}  {:s} [{:08X}]  health {:.6f}  x {:.4f}", TraceLog::Who{ actor },
						RE::TESFullName::GetFullName(*effect), effect->formID, health, mult);
				}
			}

			return g_hitEffect(a_caster, a_power * mult, a_targets, a_source, a_noHitArt, a_hostileOnly);
		}
	}

	void InstallEffect()
	{
		const auto cast = RE::ID::MagicCaster::Cast.address();

		if (!CallPatch::PatchCall(HIT_EFFECT_SITE, cast, reinterpret_cast<std::uintptr_t>(&HitEffectHk), g_hitEffect)) {
			REX::WARN("A worn weapon's object effects will keep landing at full strength.");
		} else {
			REX::INFO("A weapon's object effects land at the condition the weapon is in.");
		}
	}
}
