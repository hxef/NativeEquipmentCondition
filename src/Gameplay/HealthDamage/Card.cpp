#include "Gameplay/HealthDamage/Hooks.h"

#include "Condition/Condition.h"
#include "Condition/Excluded.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/TraceLog.h"
#include "Gameplay/HealthDamage/Curve.h"
#include "Gameplay/HealthDamage/Trace.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <iterator>
#include <span>
#include <utility>

namespace HealthDamage
{
	namespace
	{
		// -------------------------------------------------------------------
		// Where the engine works damage out
		// -------------------------------------------------------------------

		using CallPatch::AE;
		using CallPatch::CallSite;
		using CallPatch::NO_OG_COVERED;
		using CallPatch::OG;

		// Both sit inside PipboyInventoryUtils::FillDamageTypeInfo, which adds
		// a weapon's damage types up for its item card. The first calls
		// CombatFormulas::GetWeaponDisplayDamage and the second
		// BGSEntryPoint::HandleEntryPoint.
		constexpr CallSite CARD_HEALTH_SITE{ AE{ RE::ID::PipboyInventoryUtils::FillDamageTypeInfo.ae(), 0x0B6 }, OG{ RE::ID::PipboyInventoryUtils::FillDamageTypeInfo.og(), 0x0BD }, "card health" };
		constexpr CallSite CARD_TYPES_SITE{ AE{ RE::ID::PipboyInventoryUtils::FillDamageTypeInfo.ae(), 0x1B7 }, OG{ RE::ID::PipboyInventoryUtils::FillDamageTypeInfo.og(), 0x25B }, "card types" };

		// The same function adds each object effect's magnitude to its damage
		// type through 2 walks, one over the effects the mods add and one for
		// the weapon's own and its blast's. Each asks the perk system about
		// every magnitude. OG makes both calls from 1 place, so the
		// mod effects have none of their own there and the one call scales
		// both walks.
		constexpr CallSite CARD_EFFECT_SITES[] = {
			{ AE{ 2225299, 0x10A }, NO_OG_COVERED, "card mod effects" },
			{ AE{ 2225344, 0x0DA }, OG{ 282246, 0x063 }, "card effects" },
		};

		// Inside CombatFormulas::GetWeaponDisplayDamage, the one call about the
		// blast half of a weapon's damage before the 2 halves are added.
		constexpr CallSite CARD_BLAST_SITE{ AE{ RE::ID::CombatFormulas::GetWeaponDisplayDamage.ae(), 0x14D }, OG{ RE::ID::CombatFormulas::GetWeaponDisplayDamage.og(), 0x155 }, "card blast" };

		// GetWeaponDisplayDamage has 4 callers on both games. Beside the card,
		// this is the one that passes the health a stack carries. It works out
		// 1 stack's damage for the item lists, such as the workbench's. The
		// other 2 pass full health, which scales nothing.
		constexpr CallSite STACK_DAMAGE_SITE{ AE{ 2222649, 0x13E }, OG{ 307737, 0x12F }, "stack damage" };

		// -------------------------------------------------------------------
		// The item card hooks
		// -------------------------------------------------------------------

		// The condition of the item whose card is being built, handed from the
		// first Pip-Boy hook to the second. The second stands at 3 places
		// inside the one call that adds up the card, on the thread building it,
		// and the first always runs before them, so a thread local carries it.
		// The types hook takes it once, so a card whose health call another
		// DLL skipped never takes the condition of the card before it.
		thread_local float t_cardHealth = Condition::INVALID_HEALTH;

		// What the types hook took, for the same card's object effects. The
		// health hook clears it, so a card whose types call was skipped leaves
		// its effects at full too.
		thread_local float t_cardEffects = Condition::INVALID_HEALTH;

		// Whether the call to GetWeaponDisplayDamage that the card health hook
		// or the stack damage hook hands on is for a weapon on a list in
		// NEC/Exclude, for the display site inside it. That site sees only the
		// player and the instance data, never which weapon it is.
		thread_local bool t_listed = false;

		// The health the display site scaled the card by, for the blast hook
		// a few instructions later in the same call. Read once, so a card
		// whose display hook did not run shows the vanilla explosion
		// damage, never the last card's.
		thread_local float t_displayHealth = Condition::INVALID_HEALTH;

		// The 2 Pip-Boy hooks. The object effect sites read what they hand
		// on, so they ask it too.
		CallPatch::Held g_card;

		// The 3 type sites are the pair's types call and the 2 object effect
		// calls, in order.
		using CardHealth_t = float (*)(const RE::BGSObjectInstanceT<RE::TESObjectWEAP>&, const RE::TESAmmo*, float);
		using CardTypes_t = void (*)(RE::BGSEntryPoint::ENTRY_POINT, RE::Actor*, const void*, void*, float*);
		using CardBlast_t = void (*)(RE::BGSEntryPoint::ENTRY_POINT, RE::Actor*, const void*, float*);

		CallPatch::Link<CardHealth_t>                g_cardHealthLink;
		std::array<CallPatch::Link<CardTypes_t>, 3>  g_cardTypeLinks;
		CallPatch::Link<CardBlast_t>                 g_cardBlastLink;
		CallPatch::Link<CardHealth_t>                g_stackDamageLink;

		// Records the condition of the item, or -1 while the pair does not
		// run and for a weapon on a list in NEC/Exclude. The physical damage
		// is already scaled, since this call reaches CalcWeaponDamage through
		// the display site in Combat.cpp. The vanilla call keeps a_health,
		// so its base number stays vanilla.
		float CardHealthHk(const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, const RE::TESAmmo* a_ammo, float a_health)
		{
			const auto listed = a_weapon.object && Excluded::Has(*a_weapon.object);
			t_cardEffects = Condition::INVALID_HEALTH;
			t_cardHealth = g_card.Runs(g_cardHealthLink) && !listed ? a_health : Condition::INVALID_HEALTH;
			const auto was = std::exchange(t_listed, listed);
			const auto damage = g_cardHealthLink(a_weapon, a_ammo, a_health);
			t_listed = was;
			return damage;
		}

		// Marks a stack of a weapon on a list in NEC/Exclude, so its damage
		// and blast stay vanilla whatever health it carries.
		float StackDamageHk(const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, const RE::TESAmmo* a_ammo, float a_health)
		{
			const auto listed = a_weapon.object && g_stackDamageLink.Live() && Excluded::Has(*a_weapon.object);
			const auto was = std::exchange(t_listed, listed);
			const auto damage = g_stackDamageLink(a_weapon, a_ammo, a_health);
			t_listed = was;
			return damage;
		}

		// The card's damage types are scaled the same way as in combat: the
		// engine asks the perk system for one multiplier over the whole weapon
		// and multiplies every damage type by it, so returning it multiplied by
		// the condition scales all of them. The entry point is
		// kModAttackDamage, the perk entry that raises or lowers damage, and
		// the object effects go through kModSpellMagnitude. HandleEntryPoint
		// takes a variable number of arguments, 3 pointers here with the number
		// to change last, passed like a fixed list on x64.
		// I is 0 for the pair's types call, 1 and 2 for the object effect
		// calls, which stand alone and read whether the pair runs too.
		template <std::size_t I>
		void CardTypesHk(RE::BGSEntryPoint::ENTRY_POINT a_entryPoint, RE::Actor* a_perkOwner, const void* a_instance,
			void* a_unused, float* a_out)
		{
			g_cardTypeLinks[I](a_entryPoint, a_perkOwner, a_instance, a_unused, a_out);
			auto health = Condition::INVALID_HEALTH;
			auto live = false;
			if constexpr (I == 0) {
				health = std::exchange(t_cardHealth, Condition::INVALID_HEALTH);
				live = g_card.Runs(g_cardTypeLinks[0]);
				t_cardEffects = live ? health : Condition::INVALID_HEALTH;
			} else {
				health = t_cardEffects;
				live = g_card.Intact() && g_cardTypeLinks[I].Live();
			}
			if (a_out && live) {
				*a_out *= DamageMult(health);
			}
		}

		// The explosion part of the item card, the one call the engine makes
		// about it, so the only place the card can be told the shell came out
		// of a worn weapon. The entry point is the one Demolition Expert uses.
		// One pointer fewer than CardTypesHk: the weapon, then the number.
		void CardBlastHk(RE::BGSEntryPoint::ENTRY_POINT a_entryPoint, RE::Actor* a_perkOwner, const void* a_instance, float* a_out)
		{
			// Read first, so a weapon with no blast clears the note too.
			const auto health = std::exchange(t_displayHealth, Condition::INVALID_HEALTH);
			g_cardBlastLink(a_entryPoint, a_perkOwner, a_instance, a_out);
			if (!a_out || !(*a_out > 0.0F) || !g_cardBlastLink.Live()) {
				return;
			}

			// 0 is what every weapon that fires nothing explosive reports, so
			// the guard keeps the whole inventory out of the log.
			const auto mult = DamageMult(health);

			static std::atomic<std::uint64_t> last{ 0 };
			if (!Repeats(last, { health, *a_out, mult })) {
				TraceLog::Line("card", "blast  health {:.6f}  {:.2f} x {:.4f} = {:.2f}",
					health, *a_out, mult, *a_out * mult);
			}

			*a_out *= mult;
		}
	}

	float DisplayHealth(float a_condition)
	{
		t_displayHealth = t_listed ? Condition::INVALID_HEALTH : a_condition;
		return t_displayHealth;
	}

	void InstallCard()
	{
		const auto& entryPoint = RE::ID::BGSEntryPoint::HandleEntryPoint;

		g_card = CallPatch::PatchTogether({
			{ CARD_HEALTH_SITE, RE::ID::CombatFormulas::GetWeaponDisplayDamage, reinterpret_cast<std::uintptr_t>(&CardHealthHk), &g_cardHealthLink },
			{ CARD_TYPES_SITE, entryPoint, reinterpret_cast<std::uintptr_t>(&CardTypesHk<0>), &g_cardTypeLinks[0] },
		}, Part::kCardDamage);

		if (!g_card) {
			REX::WARN("The item card will keep printing damage types at full strength.");
		} else {
			REX::INFO("The item card prints damage types at the condition the item is in.");

			// The card's object effects read the condition the pair records, so
			// they go in only once the pair has.
			const auto effectHooks = CallPatch::PerSite<std::size(CARD_EFFECT_SITES)>([]<std::size_t I>() { return &CardTypesHk<1 + I>; });
			CallPatch::PatchAll(CARD_EFFECT_SITES, RE::ID::BGSEntryPoint::HandleEntryPoint, effectHooks, std::span{ g_cardTypeLinks }.subspan(1),
				"The item card prints object effects at the condition the item is in", Part::kCardDamage);
		}

		// Installed on its own, since it is in a different function and reads a
		// different value. A card that lost one is still right about the other.
		if (!CallPatch::PatchCall(CARD_BLAST_SITE, entryPoint, reinterpret_cast<std::uintptr_t>(&CardBlastHk), g_cardBlastLink, Part::kCardDamage)) {
			REX::WARN("The item card will keep printing a worn explosive weapon at full damage.");
		} else {
			REX::INFO("The item card prints the blast at the condition the weapon is in.");
		}

		// Without it a listed weapon's stack in an item list counts by a
		// health it carries, and every other number works.
		CallPatch::PatchCall(STACK_DAMAGE_SITE, RE::ID::CombatFormulas::GetWeaponDisplayDamage, reinterpret_cast<std::uintptr_t>(&StackDamageHk), g_stackDamageLink);
	}
}
