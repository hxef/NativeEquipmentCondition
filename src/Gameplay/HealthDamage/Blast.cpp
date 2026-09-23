#include "Gameplay/HealthDamage/Hooks.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Core/CallPatch.h"
#include "Core/TraceLog.h"
#include "Gameplay/HealthDamage/Curve.h"
#include "Gameplay/HealthDamage/Trace.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace HealthDamage
{
	namespace
	{
		// -------------------------------------------------------------------
		// Where the engine works damage out
		// -------------------------------------------------------------------

		using CallPatch::CallSite;

		// The 3 places the engine asks a live explosion how much damage it
		// does, through Explosion::GetDamage. A weapon that fires something
		// explosive deals 2 separate amounts: the gun hits for what
		// CalcWeaponDamage says and the shell explodes for what its explosion
		// record says, added at the end. On a Fat Man the gun is worth 18 and
		// the explosion 450, so scaling the first alone leaves a broken Fat Man
		// hitting like new. The engine works a blast out once and stores it on
		// the explosion. The sites are FindTargets deciding whether the blast
		// is strong enough to look for targets, ProcessTargets working out what
		// each loses, and the hit data being filled in.
		constexpr CallSite BLAST_SITES[] = {
			{ 2236662, 0x4BB, "targets" },
			{ RE::ID::Explosion::ProcessTargets.id(), 0x5F5, "damage" },
			{ 2236848, 0x039, "hit" },
		};

		// The call to MagicCaster::Cast inside MagicCaster::CastSpellImmediate,
		// which casts at once. A blast casts its object effect through it,
		// along with much else. See Effect.cpp.
		constexpr CallSite BLAST_EFFECT_SITE{ 2226290, 0x0AF, "blast effect" };

		// The one call to Explosion::ProcessTargets, which goes through
		// everything the blast hit.
		constexpr CallSite BLAST_TARGETS_SITE{ 2236666, 0x15E8, "blast targets" };

		constexpr std::size_t NUM_BLAST_SITES = std::size(BLAST_SITES);

		// Explosion::GetDamage: the explosion in, what it is worth out.
		using BlastDamage_t = float (*)(RE::Explosion*);

		// Who set a blast off, and how worn the weapon it came out of is.
		struct Blast
		{
			RE::NiPointer<RE::Actor> actor;
			float                    health{ Condition::INVALID_HEALTH };
		};

		// The player or any NPC, the rule EquippedHealthHk in Combat.cpp
		// applies, and the weapon the blast came out of is the one that counts.
		// A trap or a barrel exploding names no actor. A grenade, a mine, or a
		// shot that lands after the weapon was put away finds no health, and
		// DamageMult leaves that alone.
		Blast BlastOf(const RE::Explosion& a_explosion)
		{
			const auto*              weapon = a_explosion.weaponSource.object;
			const auto               owner = a_explosion.owner.get();
			RE::NiPointer<RE::Actor> actor{ weapon && owner ? owner->As<RE::Actor>() : nullptr };
			if (!actor) {
				return {};
			}

			const auto health = Equipped::WeaponHealth(actor.get(), weapon);
			return { std::move(actor), health };
		}

		// -------------------------------------------------------------------
		// The damage hooks
		// -------------------------------------------------------------------

		// Scales the blast a weapon sets off by the weapon's condition.
		// Everything the engine works out from a blast starts at this value, so
		// scaling it here reaches every target, what each loses and the armor
		// in the way, consistently. Scaled on the way out, not written back,
		// since the engine keeps the value and a reduced number would still be
		// there after a repair.
		template <std::size_t I>
		float BlastDamageHk(RE::Explosion* a_explosion)
		{
			// All 3 sites hand this a live explosion.
			const auto base = a_explosion->GetDamage();

			const auto blast = BlastOf(*a_explosion);
			if (!blast.actor) {
				return base;
			}
			const auto health = blast.health;
			const auto mult = DamageMult(health);

			// One blast asks once for itself and once per target, with the same
			// answer every time.
			if (blast.actor.get() == RE::PlayerCharacter::GetSingleton()) {
				static std::atomic<std::uint64_t> lastPlayer{ 0 };
				if (!Repeats(lastPlayer, { health, base, mult })) {
					TraceLog::Line("blast", "{:s}  health {:.6f}  {:.2f} x {:.4f} = {:.2f}",
						BLAST_SITES[I].what, health, base, mult, base * mult);
				}
			} else if (mult != 1.0F) {
				static std::atomic<std::uint64_t> lastNpc{ 0 };
				if (!Repeats(lastNpc, { health, base, mult }, NpcScope(*blast.actor))) {
					TraceLog::Npc::Line("blast", "{:s}  {:s}  health {:.6f}  {:.2f} x {:.4f} = {:.2f}",
						BLAST_SITES[I].what, TraceLog::Who{ blast.actor.get() }, health, base, mult, base * mult);
				}
			}

			return base * mult;
		}

		// Entry i holds the hook for BLAST_SITES[i], as MakeCalcHooks in
		// Combat.cpp.
		template <std::size_t... I>
		constexpr auto MakeBlastHooks(std::index_sequence<I...>)
		{
			return std::array<BlastDamage_t, sizeof...(I)>{ &BlastDamageHk<I>... };
		}

		constexpr auto BLAST_HOOKS = MakeBlastHooks(std::make_index_sequence<NUM_BLAST_SITES>{});

		// -------------------------------------------------------------------
		// The object effect hooks
		// -------------------------------------------------------------------

		// The blast whose targets this thread is going through, handed from
		// BlastTargetsHk to BlastEffectHk. The cast runs inside that loop on
		// the same thread.
		thread_local const RE::Explosion* t_blast = nullptr;

		// Marks the blast for BlastEffectHk while it damages its targets. The
		// cast names no weapon and no blast, so the mark is the only way to
		// tell.
		void BlastTargetsHk(RE::Explosion* a_explosion)
		{
			const auto outer = std::exchange(t_blast, a_explosion);
			a_explosion->ProcessTargets();
			t_blast = outer;
		}

		// Casts a blast's object effect at the condition of the weapon that set
		// it off, the rule BlastDamageHk applies to its damage. Everything else
		// cast at once keeps its power.
		bool BlastEffectHk(RE::MagicCaster* a_caster, float a_power, std::uint32_t* a_targets, RE::TESBoundObject* a_source,
			bool a_noHitArt, bool a_hostileOnly)
		{
			const auto* form = t_blast ? t_blast->GetObjectReference() : nullptr;
			const auto* explosion = form ? form->As<RE::BGSExplosion>() : nullptr;
			const auto* effect = explosion ? explosion->GetBaseEnchanting() : nullptr;
			const auto  blast = effect && a_caster->currentSpell == effect ? BlastOf(*t_blast) : Blast{};
			if (!blast.actor) {
				return a_caster->Cast(a_power, a_targets, a_source, a_noHitArt, a_hostileOnly);
			}

			const auto mult = DamageMult(blast.health);

			// A blast casts its effect once for every target it hit.
			if (blast.actor.get() == RE::PlayerCharacter::GetSingleton()) {
				static std::atomic<std::uint64_t> lastPlayer{ 0 };
				if (!Repeats(lastPlayer, { blast.health, mult }, effect->formID)) {
					TraceLog::Line("blast", "effect {:s} [{:08X}]  health {:.6f}  x {:.4f}",
						RE::TESFullName::GetFullName(*effect), effect->formID, blast.health, mult);
				}
			} else if (mult != 1.0F) {
				static std::atomic<std::uint64_t> lastNpc{ 0 };
				if (!Repeats(lastNpc, { blast.health, mult }, NpcScope(*blast.actor))) {
					TraceLog::Npc::Line("blast", "effect  {:s}  {:s} [{:08X}]  health {:.6f}  x {:.4f}",
						TraceLog::Who{ blast.actor.get() }, RE::TESFullName::GetFullName(*effect), effect->formID,
						blast.health, mult);
				}
			}

			return a_caster->Cast(a_power * mult, a_targets, a_source, a_noHitArt, a_hostileOnly);
		}
	}

	void InstallBlast()
	{
		CallPatch::PatchAll(BLAST_SITES, RE::ID::Explosion::GetDamage, CallPatch::AsAddresses(BLAST_HOOKS),
			"An explosion is worth what the weapon that set it off is worth");

		const auto cast = RE::ID::MagicCaster::Cast.address();

		// The first only marks the blast and the second reads the mark, so the
		// second is not installed alone.
		const auto blastEffect =
			CallPatch::PatchCall(BLAST_TARGETS_SITE, RE::ID::Explosion::ProcessTargets.address(), reinterpret_cast<std::uintptr_t>(&BlastTargetsHk)) &&
			CallPatch::PatchCall(BLAST_EFFECT_SITE, cast, reinterpret_cast<std::uintptr_t>(&BlastEffectHk));

		if (!blastEffect) {
			REX::ERROR("A worn weapon's blast will keep casting its object effect at full strength.");
		} else {
			REX::INFO("A blast casts its object effect at the condition of the weapon that set it off.");
		}
	}
}
