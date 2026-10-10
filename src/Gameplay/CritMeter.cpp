#include "Gameplay/CritMeter.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <limits>
#include <utility>

namespace CritMeter
{
	namespace
	{
		// The one call to CombatFormulas::CalcVATSCriticalCharge, inside the
		// function VATS calls whenever it adds charge.
		constexpr CallPatch::CallSite CHARGE_SITE{ 2237284, 0x108, "crit meter" };

		// 2 calls inside HitData::RollCritical: the perk check on the chance,
		// entry point 1, with the attacker and the weapon, and the roll itself,
		// a random number the chance is compared with.
		constexpr CallPatch::CallSite CHANCE_SITE{ 2236866, 0x1B9, "crit chance" };
		constexpr CallPatch::CallSite ROLL_SITE{ 2236866, 0x22A, "crit roll" };

		// A straight line from fCritMeterFloor at nothing to 1 at full, the
		// shape of HealthDamage/Curve.h. Lower than the damage and fire rate
		// floors, since a critical is a reward and not something a fight turns
		// on. fCritMeterFloor keeps no clamp for a hand edit, as the MCM slider
		// stops at 0 and 1.
		float ChargeMult(float a_health)
		{
			return Condition::Share(a_health, Settings::fCritMeterFloor.GetValue());
		}

		CallPatch::Link<float(const RE::ActorValueOwner*, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>&)> g_chargeLink;

		float CritChargeHk(const RE::ActorValueOwner* a_avOwner, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon)
		{
			const auto base = g_chargeLink(a_avOwner, a_weapon);

			// Nothing to scale in an empty charge. Unarmed names the race's
			// bare hands weapon, and the condition lookup below leaves those
			// alone.
			if (base <= 0.0F || !a_weapon.object || !Settings::bCritMeter.GetValue() || !g_chargeLink.Live()) {
				return base;
			}

			// The charge is the player's, and the weapon is the one the engine
			// just read out of the player's hands.
			const auto health = Equipped::WeaponHealth(RE::PlayerCharacter::GetSingleton(), a_weapon.object);
			const auto mult = ChargeMult(health);
			if (mult == 1.0F) {
				return base;
			}

			TraceLog::Line("crit meter", "{:s}  health {:.6f}  {:.4f} x {:.4f} = {:.4f} of a meter",
				TraceLog::Who{ a_weapon.object }, health, base, mult, base * mult);

			return base * mult;
		}

		// The NPC blow the roll is for, handed from the chance hook to the roll
		// hook. The chance is the roll's own number, which the perks change in
		// place. Both hooks sit inside one call on one thread, so a thread
		// local carries the blow. The player's blows, and weapons with no
		// condition, leave it empty.
		struct Blow
		{
			RE::Actor*               attacker{ nullptr };
			const RE::TESObjectWEAP* weapon{ nullptr };
			const float*             chance{ nullptr };
		};

		thread_local Blow t_blow{};

		// The chance and roll hooks.
		CallPatch::Held                                                                                                     g_rolled;
		CallPatch::Link<void(RE::BGSEntryPoint::ENTRY_POINT, RE::Actor*, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>*, const void*, float*)> g_chanceLink;
		CallPatch::Link<float()>                                                                                            g_rollLink;

		// Stands in for BGSEntryPoint::HandleEntryPoint where the roll asks the
		// perks about the chance, and notes whose blow it is. The arguments are
		// the attacker, the weapon, the target as an object instance and the
		// chance in percent, see HealthDamage/Card.cpp.
		void ChanceHk(RE::BGSEntryPoint::ENTRY_POINT a_entryPoint, RE::Actor* a_attacker,
			const RE::BGSObjectInstanceT<RE::TESObjectWEAP>* a_weapon, const void* a_target, float* a_chance)
		{
			g_chanceLink(a_entryPoint, a_attacker, a_weapon, a_target, a_chance);

			const auto* object = a_weapon ? a_weapon->object : nullptr;
			const auto* weapon = object && object->IsWeapon() ? static_cast<const RE::TESObjectWEAP*>(object) : nullptr;
			const bool  npc = a_attacker && a_attacker != RE::PlayerCharacter::GetSingleton();
			// The set is asked before the switch, so a pair that waits still
			// sees this hook run while bCritMeter is off.
			const bool runs = g_rolled.Runs(g_chanceLink);
			t_blow = runs && Settings::bCritMeter.GetValue() && npc && weapon && Condition::WearsOut(*weapon) ? Blow{ a_attacker, weapon, a_chance } : Blow{};
		}

		// Stands in for BSRandom::Float0To1 where the roll is made. The blow is
		// a critical when its chance is at least the roll, so returning the
		// roll divided by the share has the same effect as multiplying the
		// chance by it.
		float RollHk()
		{
			const auto roll = g_rollLink();
			const auto blow = std::exchange(t_blow, Blow{});
			if (!g_rolled.Runs(g_rollLink) || !blow.attacker || !blow.chance || !(*blow.chance > 0.0F)) {
				return roll;
			}

			// Read as the damage type hook in HealthDamage/Combat.cpp reads it.
			const auto health = Equipped::WeaponHealth(blow.attacker, blow.weapon);
			const auto mult = ChargeMult(health);
			const auto chance = *blow.chance;

			// A share of 0 or less, which a floor of 0 at nothing or a floor
			// below 0 reaches, leaves no chance at all.
			const auto scaled = mult > 0.0F ? roll / mult : std::numeric_limits<float>::infinity();

			// Whether it landed is the engine's own test, which follows this
			// call.
			TraceLog::Npc::Line("crit", "{:s} with {:s}  health {:.6f}  chance {:.2f} x {:.4f} = {:.2f}  {:s}",
				TraceLog::Who{ blow.attacker }, TraceLog::Who{ blow.weapon }, health, chance, mult, chance * mult,
				chance * 0.01F >= scaled ? "critical"sv : "no critical"sv);
			return scaled;
		}
	}

	void Install()
	{
		const auto floor = Settings::fCritMeterFloor.GetValue();

		// Both are noted before either is judged, so each mod that has one is
		// named. A held pair is never written while the meter is another
		// mod's, so its hooks give the game's own result.
		const auto meter = CallPatch::PatchCall(CHARGE_SITE, RE::ID::CombatFormulas::CalcVATSCriticalCharge.address(),
			reinterpret_cast<std::uintptr_t>(&CritChargeHk), g_chargeLink);

		// The roll reads what the chance noted, so the 2 go in together. A
		// smaller part: without them an NPC's worn weapon lands criticals as
		// often as a new one, and the meter still fills slower.
		g_rolled = CallPatch::PatchTogether({
			{ CHANCE_SITE, RE::ID::BGSEntryPoint::HandleEntryPoint.address(), reinterpret_cast<std::uintptr_t>(&ChanceHk), &g_chanceLink },
			{ ROLL_SITE, RE::ID::BSRandom::Float0To1.address(), reinterpret_cast<std::uintptr_t>(&RollHk), &g_rollLink },
		}, Part::kNpcCrits);

		if (!meter) {
			REX::WARN("A worn weapon will keep filling the VATS critical meter as fast as a new one.");
			return;
		}
		REX::INFO("A worn weapon fills the VATS critical meter slower, down to {:.2f} of the rate at nothing.", floor);
		if (!g_rolled) {
			REX::WARN("An NPC's worn weapon will keep landing critical hits as often as a new one.");
		} else {
			REX::INFO("An NPC's worn weapon lands fewer critical hits, down to {:.2f} of the chance at nothing.", floor);
		}
	}
}
