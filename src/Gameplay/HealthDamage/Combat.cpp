#include "Gameplay/HealthDamage/Hooks.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/TraceLog.h"
#include "Gameplay/HealthDamage/Curve.h"
#include "Gameplay/HealthDamage/Trace.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <span>
#include <string_view>
#include <utility>

namespace HealthDamage
{
	namespace
	{
		// -------------------------------------------------------------------
		// Where the engine works damage out
		// -------------------------------------------------------------------

		// Every site below is an Address Library ID and an offset, see
		// CallPatch.h. InstallCombat names the function each one calls.
		using CallPatch::CallSite;

		// The calls to CombatFormulas::CalcWeaponDamage. Every point of
		// physical weapon damage comes out of it, for the player and every NPC,
		// and so does the number the Pip-Boy prints.
		constexpr CallSite CALC_SITES[] = {
			{ 2209000, 0x010, "shim" },
			{ RE::ID::CombatFormulas::GetWeaponDisplayDamage.id(), 0x059, "display" },
			{ 2236845, 0x37A, "attack1" },
			{ 2237056, 0x152, "attack2" },
		};

		// The calls to Actor::GetEquippedItemHealth, the engine's own answer to
		// how worn the weapon an actor swings is. Only these 2 combat paths
		// call it.
		constexpr CallSite HEALTH_SITES[] = {
			{ 2236845, 0x2C8, "attack1 health" },
			{ 2237056, 0x10D, "attack2 health" },
		};

		// The 3 places the engine walks a weapon's damage type list, through
		// HitData::ApplyDamageTypes. Physical damage is one number out of
		// CalcWeaponDamage. The rest is a list of damage type forms and values
		// on the instance data, where a laser's energy and the Cryolator's cryo
		// live, each resisted and added to the hit. The first site is the
		// ordinary hit path, the other 2 are similar paths for attacks whose
		// damage is already worked out.
		constexpr CallSite TYPE_SITES[] = {
			{ 2236845, 0x3EA, "types1" },
			{ 2236846, 0x19F, "types2" },
			{ 2236849, 0x0BF, "types3" },
		};

		constexpr std::size_t NUM_CALC_SITES = std::size(CALC_SITES);
		constexpr std::size_t NUM_TYPE_SITES = std::size(TYPE_SITES);

		// Which site stands inside GetWeaponDisplayDamage. The blast half of
		// the card is worked out a few instructions later with no condition of
		// its own, so this site's is kept for it.
		constexpr std::size_t DISPLAY_SITE = 1;
		static_assert(std::string_view{ CALC_SITES[DISPLAY_SITE].what } == "display");

		// Which site stands inside the shim around CalcWeaponDamage. Its only
		// caller passes full condition, so its hook changes no number and only
		// feeds the trace logs.
		constexpr std::size_t SHIM_SITE = 0;
		static_assert(std::string_view{ CALC_SITES[SHIM_SITE].what } == "shim");

		// Where the attack sites start. Each goes in with the health site of
		// the same attack, attack1 with HEALTH_SITES[0] and attack2 with
		// HEALTH_SITES[1], see InstallCombat.
		constexpr std::size_t FIRST_ATTACK_SITE = 2;
		static_assert(std::string_view{ CALC_SITES[FIRST_ATTACK_SITE].what } == "attack1");
		static_assert(NUM_CALC_SITES - FIRST_ATTACK_SITE == std::size(HEALTH_SITES));

		// -------------------------------------------------------------------
		// The shape of each call
		// -------------------------------------------------------------------

		// Every hook takes the arguments of the function it stands in for, in
		// the same order. A member function's object comes first.

		// The game's own signature from CommonLibF4, so the hooks and the call
		// can never disagree. The first argument is whoever the damage belongs
		// to, the attacking actor at both attack sites. The engine takes a base
		// NPC record there too and reads the form type to find the actor
		// values, so it is a form, and comparing it against the player is
		// exact.
		using CalcWeaponDamage_t = decltype(&RE::CombatFormulas::CalcWeaponDamage);

		// HitData::ApplyDamageTypes: the hit, the weapon, the multiplier.
		using ApplyDamageTypes_t = void (*)(RE::HitData*, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>&, float);

		// Actor::GetEquippedItemHealth: the actor and the equip slot.
		using EquippedHealth_t = float (*)(RE::Actor*, RE::BGSEquipIndex);

		// -------------------------------------------------------------------
		// Applying the curve
		// -------------------------------------------------------------------

		// Whoever struck a blow, when the blow names a weapon. Everybody
		// counts, the rule EquippedHealthHk applies to physical damage.
		RE::NiPointer<RE::Actor> AttackerOf(const RE::HitData& a_hit)
		{
			return a_hit.weapon.object ? a_hit.aggressor.get() : nullptr;
		}

		// What the condition of the weapon behind a blow leaves of the damage.
		// Only the weapon the hit names counts, so a grenade, a mine or a shot
		// fired before a weapon swap finds no health and keeps full damage.
		float AttackerMult(const RE::HitData& a_hit, RE::Actor& a_attacker)
		{
			return DamageMult(Equipped::WeaponHealth(&a_attacker, a_hit.weapon.object));
		}

		// -------------------------------------------------------------------
		// Saying so in the trace logs
		// -------------------------------------------------------------------

		// The NPC half of Trace, for the NPC log. Only a blow the condition
		// changed is written, since claws, turrets and everything without a
		// condition hit as ever, several times a second.
		template <std::size_t I>
		void TraceNpc(const RE::TESForm* a_source, float a_health, float a_base, float a_mult)
		{
			if (!a_source || a_mult == 1.0F) {
				return;
			}

			// Counted apart from the player's calls and once per call site,
			// since each instantiation has statics of its own.
			static std::atomic<std::uint32_t> calls{ 0 };
			static std::atomic<std::uint64_t> last{ 0 };

			const auto seen = calls.fetch_add(1, std::memory_order_relaxed) + 1;
			if (Repeats(last, { a_health, a_base, a_mult }, NpcScope(*a_source))) {
				return;
			}

			TraceLog::Npc::Line("damage", "{:s}  {:s}  health {:.6f}  {:.2f} x {:.4f} = {:.2f}  (call {:d})",
				CALC_SITES[I].what, TraceLog::Who{ a_source }, a_health, a_base, a_mult, a_base * a_mult, seen);
		}

		// Once per attack for every actor in the game. The player's blows go in
		// the game log and everybody else's in the NPC log, a repeated line is
		// left out, see Repeats in Trace.h, and the count on the line shows a
		// dropped call as a jump.
		template <std::size_t I>
		void Trace(const RE::TESForm* a_source, float a_health, float a_base, float a_mult)
		{
			// The display site is the Pip-Boy working out a card, with no
			// attacker.
			const auto card = std::string_view{ CALC_SITES[I].what } == "display";
			if (!card && a_source != static_cast<const RE::TESForm*>(RE::PlayerCharacter::GetSingleton())) {
				TraceNpc<I>(a_source, a_health, a_base, a_mult);
				return;
			}

			// Several threads reach these hooks, so the counter is atomic.
			// Order does not matter.
			static std::atomic<std::uint32_t> calls{ 0 };

			const auto seen = calls.fetch_add(1, std::memory_order_relaxed) + 1;

			// An item with no condition at x 1.0000 changes nothing, and the
			// Pip-Boy works one out for every such item.
			if (a_health < 0.0F && a_mult == 1.0F) {
				return;
			}

			// Each call site keeps its own last value, since each is its own
			// instantiation.
			static std::atomic<std::uint64_t> last{ 0 };
			if (Repeats(last, { a_health, a_base, a_mult })) {
				return;
			}

			TraceLog::Line(card ? "card"sv : "damage"sv, "{:s}  health {:.6f}  {:.2f} x {:.4f} = {:.2f}  (call {:d})",
				CALC_SITES[I].what, a_health, a_base, a_mult, a_base * a_mult, seen);
		}

		// -------------------------------------------------------------------
		// The damage hooks
		// -------------------------------------------------------------------

		// The condition of the weapon the item card is drawn for, handed from
		// the display site to CardBlastHk in Card.cpp through DisplayHealth.
		// Both sit inside one function on one thread. The note is read once,
		// so a card whose display hook did not run shows the game's own
		// explosion damage, never the last card's.
		thread_local float t_displayHealth = Condition::INVALID_HEALTH;

		// Each attack's 2 hooks, see InstallCombat.
		std::array<CallPatch::Held, std::size(HEALTH_SITES)> g_attacks;

		std::array<CallPatch::Link<CalcWeaponDamage_t>, NUM_CALC_SITES> g_calcLinks;
		std::array<CallPatch::Link<ApplyDamageTypes_t>, NUM_TYPE_SITES> g_typeLinks;
		std::array<CallPatch::Link<EquippedHealth_t>, std::size(HEALTH_SITES)> g_healthLinks;

		// Whether an attack's health hook ran on this blow, one per attack on
		// this thread. The calc hook scales only a blow its health hook ran,
		// so a mod that hands the health call on only sometimes leaves the
		// blows it skips at the game's own damage, never the floor.
		thread_local std::array<bool, std::size(HEALTH_SITES)> t_health{};

		// One hook per call site. Each instantiation of a template is a
		// separate function with its own statics, so each site keeps its own
		// trace state and knows its index.
		template <std::size_t I>
		float CalcWeaponDamageHk(const RE::TESForm* a_source, const RE::TESObjectWEAP::InstanceData* a_data,
			const RE::TESAmmo* a_ammo, float a_condition, float a_rangeMult)
		{
			const auto base = g_calcLinks[I](a_source, a_data, a_ammo, a_condition, a_rangeMult);
			if constexpr (I >= FIRST_ATTACK_SITE) {
				const auto attack = I - FIRST_ATTACK_SITE;
				// Runs marks the proof for this site either way, so a shared
				// pair comes back. The blow is scaled only when its health
				// hook ran, see t_health.
				const auto live = g_attacks[attack].Runs(g_calcLinks[I]);
				if (!std::exchange(t_health[attack], false) || !live) {
					return base;
				}
			} else if (!g_calcLinks[I].Live()) {
				return base;
			}

			// The player's health comes from EquippedHealthHk, which reads the
			// weapon in hand even while a grenade is thrown. A grenade or a
			// mine has no condition, so its damage stays.
			const auto thrown = a_data && (a_data->type == RE::WEAPON_TYPE::kGrenade || a_data->type == RE::WEAPON_TYPE::kMine);
			const auto mult = thrown ? 1.0F : DamageMult(a_condition);

			// Kept for the blast half of the same card, see t_displayHealth.
			if constexpr (I == DISPLAY_SITE) {
				t_displayHealth = a_condition;
			}

			Trace<I>(a_source, a_condition, base, mult);
			return base * mult;
		}

		// The hook at the shim site, see SHIM_SITE. It hands the game's own
		// number back.
		float ShimHk(const RE::TESForm* a_source, const RE::TESObjectWEAP::InstanceData* a_data, const RE::TESAmmo* a_ammo,
			float a_condition, float a_rangeMult)
		{
			const auto base = g_calcLinks[SHIM_SITE](a_source, a_data, a_ammo, a_condition, a_rangeMult);
			if (TraceLog::IsOpen()) {
				Trace<SHIM_SITE>(a_source, a_condition, base, 1.0F);
			}
			return base;
		}

		// Hooks the engine's damage type walk. The engine already passes the
		// range falloff into it as a multiplier, the same one CalcWeaponDamage
		// gets for the physical damage, so multiplying the condition into it
		// scales every damage type at once without knowing what any of them
		// are.
		template <std::size_t I>
		void ApplyDamageTypesHk(RE::HitData* a_hit, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, float a_mult)
		{
			// A blow marked as a base damage prediction is a question, not a
			// blow: every NPC in a fight keeps rating its weapons with a blow
			// that never lands. HitData::Populate skips the condition for such
			// a blow, so this half is left alone too, and so is the log.
			if (!g_typeLinks[I].Live() || a_hit->flags.any(RE::HitData::Flag::kPredictBaseDamage)) {
				g_typeLinks[I](a_hit, a_weapon, a_mult);
				return;
			}

			const auto attacker = AttackerOf(*a_hit);
			const auto mult = attacker ? AttackerMult(*a_hit, *attacker) : 1.0F;

			// The same log line as the damage hook, to the same 2 logs.
			if (attacker.get() == RE::PlayerCharacter::GetSingleton()) {
				static std::atomic<std::uint64_t> lastPlayer{ 0 };
				if (!Repeats(lastPlayer, { mult })) {
					TraceLog::Line("types", "{:s}  x {:.4f}", TYPE_SITES[I].what, mult);
				}
			} else if (attacker && mult != 1.0F) {
				static std::atomic<std::uint64_t> lastNpc{ 0 };
				if (!Repeats(lastNpc, { mult }, NpcScope(*attacker))) {
					TraceLog::Npc::Line("types", "{:s}  {:s}  x {:.4f}", TYPE_SITES[I].what,
						TraceLog::Who{ attacker.get() }, mult);
				}
			}

			g_typeLinks[I](a_hit, a_weapon, a_mult * mult);
		}

		// -------------------------------------------------------------------
		// What combat reads as the weapon in hand
		// -------------------------------------------------------------------

		// Stands in for Actor::GetEquippedItemHealth. The original works out
		// the slot from an object instance built around a null form, so it
		// stops at the first equipped weapon with the low slot bit set, which
		// on the player is not the weapon in hand, and returns a flat 1.0.
		// Nothing in the unmodified game writes item health, so the bug never
		// showed. Equipped::WeaponHealth picks the equipped weapon that takes
		// part, the same stack the wear is written to. When it finds none the
		// original still runs, so every actor without such a weapon keeps
		// vanilla behaviour.
		// One per attack, P being its index in HEALTH_SITES.
		template <std::size_t P>
		float EquippedHealthHk(RE::Actor* a_actor, RE::BGSEquipIndex a_equipIndex)
		{
			// Runs marks the proof for this site so a shared pair comes back.
			// While the pair does not run, the game's own answer goes on and the
			// calc hook leaves the blow alone, see t_health.
			if (!g_attacks[P].Runs(g_healthLinks[P])) {
				t_health[P] = false;
				return g_healthLinks[P](a_actor, a_equipIndex);
			}

			// Every actor's condition reaches combat. Only the player's weapons
			// wear, see WeaponEvents.h, so an NPC fights at the condition its
			// weapon spawned in. 0 is a real health, so -1 means there is none.
			const auto ours = Equipped::WeaponHealth(a_actor);
			const auto mine = ours >= 0.0F;

			auto result = ours;
			if (!mine) {
				result = g_healthLinks[P](a_actor, a_equipIndex);

				// The one place 0 can mean something other than broken: the
				// engine's walk returns 0 when it finds nothing, which here
				// means full condition. After this 0 means broken.
				if (!(result > 0.0F)) {
					result = 1.0F;
				}
			}

			// The calc hook of this blow scales only a blow its health hook
			// ran, see t_health.
			t_health[P] = true;
			if (a_actor == RE::PlayerCharacter::GetSingleton()) {
				TraceLog::Line("health", "equipped {:.6f} from {:s}", result, mine ? "NEC" : "engine");
			} else {
				TraceLog::Npc::Line("health", "{:s} equipped {:.6f} from {:s}", TraceLog::Who{ a_actor }, result,
					mine ? "NEC" : "engine");
			}
			return result;
		}
	}

	float DisplayHealth()
	{
		return std::exchange(t_displayHealth, Condition::INVALID_HEALTH);
	}

	void InstallCombat()
	{
		const auto calc = RE::ID::CombatFormulas::CalcWeaponDamage.address();
		const auto calcHooks = CallPatch::PerSite<NUM_CALC_SITES>([]<std::size_t I>() {
			if constexpr (I == SHIM_SITE) {
				return &ShimHk;
			} else {
				return &CalcWeaponDamageHk<I>;
			}
		});
		CallPatch::PatchCall(CALC_SITES[SHIM_SITE], calc, calcHooks[SHIM_SITE], g_calcLinks[SHIM_SITE], Part::kTrace);
		CallPatch::PatchAll(std::span{ CALC_SITES }.subspan<DISPLAY_SITE, 1>(), RE::ID::CombatFormulas::CalcWeaponDamage,
			std::span{ calcHooks }.subspan<DISPLAY_SITE, 1>(), std::span{ g_calcLinks }.subspan<DISPLAY_SITE, 1>(),
			"Weapon damage outside combat scales with condition", Part::kCardDamage);

		// An attack's damage hook takes the condition its health hook returns.
		// The engine's own health call answers 0 for a creature or a bare fist,
		// which the damage hook would read as broken, so each attack's 2 calls
		// go in together or not at all.
		const auto health = RE::ID::Actor::GetEquippedItemHealth.address();
		const auto healthHooks = CallPatch::PerSite<std::size(HEALTH_SITES)>([]<std::size_t P>() { return &EquippedHealthHk<P>; });
		std::size_t attacks = 0;
		for (std::size_t i = 0; i < std::size(HEALTH_SITES); i++) {
			const auto site = FIRST_ATTACK_SITE + i;
			g_attacks[i] = CallPatch::PatchTogether({ { CALC_SITES[site], calc, calcHooks[site], &g_calcLinks[site] },
				{ HEALTH_SITES[i], health, healthHooks[i], &g_healthLinks[i] } });
			if (g_attacks[i]) {
				attacks++;
			}
		}
		if (attacks == 0) {
			REX::WARN("Worn weapons will hit at full damage in combat.");
		} else {
			REX::INFO("Weapon damage in combat scales with condition: {:d} of {:d} pairs of call sites.", attacks,
				std::size(HEALTH_SITES));
		}

		const auto typeHooks = CallPatch::PerSite<NUM_TYPE_SITES>([]<std::size_t I>() { return &ApplyDamageTypesHk<I>; });
		CallPatch::PatchAll(TYPE_SITES, RE::ID::HitData::ApplyDamageTypes, typeHooks, g_typeLinks, "Damage types scale with condition");
	}
}
