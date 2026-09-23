#include "Gameplay/HealthDamage/Hooks.h"

#include "Condition/Condition.h"
#include "Core/CallPatch.h"
#include "Core/TraceLog.h"
#include "Gameplay/HealthDamage/Curve.h"
#include "Gameplay/HealthDamage/Trace.h"

#include <atomic>
#include <cstdint>
#include <iterator>

namespace HealthDamage
{
	namespace
	{
		// -------------------------------------------------------------------
		// Where the engine works damage out
		// -------------------------------------------------------------------

		using CallPatch::CallSite;

		// Both sit inside PipboyInventoryUtils::FillDamageTypeInfo, which adds
		// a weapon's damage types up for its item card. The first calls
		// CombatFormulas::GetWeaponDisplayDamage and the second
		// BGSEntryPoint::HandleEntryPoint.
		constexpr CallSite CARD_HEALTH_SITE{ RE::ID::PipboyInventoryUtils::FillDamageTypeInfo.id(), 0x0B6, "card health" };
		constexpr CallSite CARD_TYPES_SITE{ RE::ID::PipboyInventoryUtils::FillDamageTypeInfo.id(), 0x1B7, "card types" };

		// The same function adds each object effect's magnitude to its damage
		// type through 2 walks, one over the effects the mods add and one for
		// the weapon's own and its blast's. Each asks the perk system about
		// every magnitude.
		constexpr CallSite CARD_EFFECT_SITES[] = {
			{ 2225299, 0x10A, "card mod effects" },
			{ 2225344, 0x0DA, "card effects" },
		};

		// Inside CombatFormulas::GetWeaponDisplayDamage, the one call about the
		// blast half of a weapon's damage before the 2 halves are added.
		constexpr CallSite CARD_BLAST_SITE{ RE::ID::CombatFormulas::GetWeaponDisplayDamage.id(), 0x14D, "card blast" };

		// -------------------------------------------------------------------
		// The item card hooks
		// -------------------------------------------------------------------

		// The condition of the item whose card is being built, handed from the
		// first Pip-Boy hook to the second. The second stands at 3 places
		// inside the one call that adds up the card, on the thread building it,
		// and the first always runs before them, so a thread local carries it.
		thread_local float t_cardHealth = Condition::INVALID_HEALTH;

		// Records the condition of the item, then changes nothing. The physical
		// damage is already scaled, since this call reaches CalcWeaponDamage
		// through the display site in Combat.cpp.
		float CardHealthHk(const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, const RE::TESAmmo* a_ammo, float a_health)
		{
			t_cardHealth = a_health;
			return RE::CombatFormulas::GetWeaponDisplayDamage(a_weapon, a_ammo, a_health);
		}

		// The card's damage types are scaled the same way as in combat: the
		// engine asks the perk system for one multiplier over the whole weapon
		// and multiplies every damage type by it, so returning it multiplied by
		// the condition scales all of them. The entry point is
		// kModAttackDamage, the perk entry that raises or lowers damage, and
		// the object effects go through kModSpellMagnitude. HandleEntryPoint
		// takes a variable number of arguments, 3 pointers here with the number
		// to change last, passed like a fixed list on x64.
		void CardTypesHk(RE::BGSEntryPoint::ENTRY_POINT a_entryPoint, RE::Actor* a_perkOwner, const void* a_instance,
			void* a_unused, float* a_out)
		{
			RE::BGSEntryPoint::HandleEntryPoint(a_entryPoint, a_perkOwner, a_instance, a_unused, a_out);
			if (a_out) {
				*a_out *= DamageMult(t_cardHealth);
			}
		}

		// The explosion part of the item card, the one call the engine makes
		// about it, so the only place the card can be told the shell came out
		// of a worn weapon. The entry point is the one Demolition Expert uses.
		// One pointer fewer than CardTypesHk: the weapon, then the number.
		void CardBlastHk(RE::BGSEntryPoint::ENTRY_POINT a_entryPoint, RE::Actor* a_perkOwner, const void* a_instance, float* a_out)
		{
			RE::BGSEntryPoint::HandleEntryPoint(a_entryPoint, a_perkOwner, a_instance, a_out);
			if (!a_out || !(*a_out > 0.0F)) {
				return;
			}

			// 0 is what every weapon that fires nothing explosive reports, so
			// the guard keeps the whole inventory out of the log.
			const auto mult = DamageMult(DisplayHealth());

			static std::atomic<std::uint64_t> last{ 0 };
			if (!Repeats(last, { DisplayHealth(), *a_out, mult })) {
				TraceLog::Line("card", "blast  health {:.6f}  {:.2f} x {:.4f} = {:.2f}",
					DisplayHealth(), *a_out, mult, *a_out * mult);
			}

			*a_out *= mult;
		}
	}

	void InstallCard()
	{
		const auto entryPoint = RE::ID::BGSEntryPoint::HandleEntryPoint.address();

		const auto card =
			CallPatch::PatchCall(CARD_HEALTH_SITE, RE::ID::CombatFormulas::GetWeaponDisplayDamage.address(), reinterpret_cast<std::uintptr_t>(&CardHealthHk)) &&
			CallPatch::PatchCall(CARD_TYPES_SITE, entryPoint, reinterpret_cast<std::uintptr_t>(&CardTypesHk));

		if (!card) {
			REX::ERROR("The item card will keep printing damage types at full strength.");
		} else {
			REX::INFO("The item card prints damage types at the condition the item is in.");

			// The card's object effects read the condition the pair records, so
			// they go in only once the pair has.
			CallPatch::PatchAll(CARD_EFFECT_SITES, RE::ID::BGSEntryPoint::HandleEntryPoint,
				CallPatch::Repeat<std::size(CARD_EFFECT_SITES)>(reinterpret_cast<std::uintptr_t>(&CardTypesHk)),
				"The item card prints object effects at the condition the item is in");
		}

		// Installed on its own, since it is in a different function and reads a
		// different value. A card that lost one is still right about the other.
		if (!CallPatch::PatchCall(CARD_BLAST_SITE, entryPoint, reinterpret_cast<std::uintptr_t>(&CardBlastHk))) {
			REX::ERROR("The item card will keep printing a worn explosive weapon at full damage.");
		} else {
			REX::INFO("The item card prints the blast at the condition the weapon is in.");
		}
	}
}
