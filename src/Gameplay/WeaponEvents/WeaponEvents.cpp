#include "Gameplay/WeaponEvents/WeaponEvents.h"

#include "Condition/Condition.h"
#include "Condition/WeaponWear/WeaponWear.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/ItemCards.h"
#include "Core/TraceLog.h"
#include "Gameplay/FireRate/FireRate.h"
#include "Gameplay/Jam.h"
#include "Gameplay/WeaponEvents/Hits.h"

#include <algorithm>
#include <cstdint>

namespace WeaponEvents
{
	namespace
	{
		// The one call to TESObjectWEAP::Fire inside WeaponFireHandler::Handle,
		// the function the weaponFire animation event runs. See FireHk.
		constexpr CallPatch::CallSite FIRE_SITE{ 2235360, 0x11D, "fire" };

		CallPatch::Link<void(const RE::BGSObjectInstanceT<RE::TESObjectWEAP>*, RE::TESObjectREFR*, std::uint32_t, RE::TESAmmo*, RE::AlchemyItem*)> g_fire;

		// The player's shot while Fire sends it on its way. Power is what its
		// rounds carry between them, see ShotSink.
		struct Shot
		{
			bool  sent{ false };
			float power{ 0.0F };
		};

		// Whoever the Fire on this thread is firing for, see Shooter, and the
		// player's shot it is sending, if any. One per thread.
		thread_local const RE::TESObjectREFR* t_shooter = nullptr;
		thread_local Shot*                    t_shot = nullptr;

		// Adds up the power of every round the player's shot sends. Fire
		// launches the rounds and then raises this event, on its own thread,
		// before it returns. A round's damage is the gun's times its power, so
		// the rounds of an ordinary shot add up to 1: a shotgun's pellets
		// share it out, a Gauss rifle's round carries the share of a full
		// charge it was held to, and a Laser Musket's round 1 for every crank.
		class ShotSink : public RE::BSTEventSink<RE::WeaponFiredEvent>
		{
		public:
			F4_HEAP_REDEFINE_NEW(ShotSink);

		private:
			RE::BSEventNotifyControl ProcessEvent(const RE::WeaponFiredEvent& a_event, RE::BSTEventSource<RE::WeaponFiredEvent>*) override
			{
				if (!t_shot || !a_event.projectiles) {
					return RE::BSEventNotifyControl::kContinue;
				}

				t_shot->sent = true;
				for (const auto& round : *a_event.projectiles) {
					if (round) {
						t_shot->power += std::max(round->power, 0.0F);
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// Wears the player's gun by the shot Fire just sent, at the power its
		// rounds carried. A shot whose event never came, which another sink can
		// stop, counts as an ordinary one. The engine has released the
		// inventory lock by the time Wear returns, which makes the refresh
		// safe, see ItemCards.h.
		void WearByShot(RE::PlayerCharacter& a_player, RE::TESObjectWEAP& a_weapon, const Shot& a_shot)
		{
			const auto power = a_shot.sent ? a_shot.power : 1.0F;
			if (power <= 0.0F) {
				TraceLog::Line("wear", "the shot left with no power, so no wear");
				return;
			}
			if (WeaponWear::Wear(a_player, a_weapon, "fire", power)) {
				ItemCards::Refresh(RE::ENUM_FORM_ID::kWEAP);
			}
		}

		// Stands in for TESObjectWEAP::Fire when the weaponFire animation event
		// fires a weapon. The player's shot can jam here first and never reach
		// Fire, see Jam.h. The equip slot arrives as a plain number, and the
		// poison the rounds carry is always null here.
		void FireHk(const RE::BGSObjectInstanceT<RE::TESObjectWEAP>* a_weapon, RE::TESObjectREFR* a_source,
			std::uint32_t a_equipIndex, RE::TESAmmo* a_ammo, RE::AlchemyItem* a_poison)
		{
			if (!g_fire.Live()) {
				g_fire(a_weapon, a_source, a_equipIndex, a_ammo, a_poison);
				return;
			}

			// The player's gun, when this shot wears it.
			RE::TESObjectWEAP* worn = nullptr;

			// Once per shot for every actor, so the player is picked out.
			// Throwing a grenade sends the same event as pulling a trigger, and
			// a_weapon is what was actually fired, so the gun in hand does not
			// wear for every Molotov thrown.
			auto* object = a_weapon ? a_weapon->object : nullptr;
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (player && a_source == player && object && object->IsWeapon()) {
				auto& weapon = static_cast<RE::TESObjectWEAP&>(*object);

				// Guns only. Melee wears from the hit sink in Hits.cpp, and
				// counting a swing in both would wear it twice.
				auto why = Condition::WhyNoCondition(weapon);
				if (!why && weapon.weaponData.type.get() != RE::WEAPON_TYPE::kGun) {
					why = "not a gun";
				}

				// Every shot opens a block in the trace log, so its wear, its
				// damage and its hits read as one event.
				const auto name = RE::TESFullName::GetFullName(weapon);
				const auto tag = weapon.IsThrownWeapon() ? "THROW"sv : "SHOT"sv;
				if (why) {
					TraceLog::Begin(tag, "{:s} [{:08X}], no wear, {:s}", name, weapon.formID, why);
				} else {
					TraceLog::Begin(tag, "{:s} [{:08X}]", name, weapon.formID);

					// A jammed shot never reaches the engine, so it costs no
					// wear.
					if (Jam::Roll(*player, *a_weapon, a_equipIndex)) {
						return;
					}
					worn = &weapon;
				}
			} else if (a_source && object && object->IsWeapon() &&
				Condition::WearsOut(static_cast<const RE::TESObjectWEAP&>(*object))) {
				// Somebody else's shot. It wears nothing, but it hits at the
				// weapon's condition, so it opens a block in the NPC log. A
				// weapon with no condition is passed over.
				const auto& weapon = static_cast<const RE::TESObjectWEAP&>(*object);
				TraceLog::Npc::Begin("SHOT"sv, "{:s} with {:s} [{:08X}]", TraceLog::Who{ a_source },
					RE::TESFullName::GetFullName(weapon), weapon.formID);

				// It fires at that condition too, so the pace is read here,
				// ahead of Fire and its sound, see FireRate.h.
				if (auto* actor = a_source->As<RE::Actor>()) {
					FireRate::NoteNpcWeapon(*actor, *a_weapon);
				}
			}

			// The shot counts as a_source's while Fire runs, see Shooter, and a
			// shot that wears is measured as it goes, see ShotSink. Both are
			// restored afterwards.
			Shot        shot;
			const auto* outerShooter = t_shooter;
			auto*       outerShot = t_shot;
			t_shooter = a_source;
			t_shot = worn ? &shot : nullptr;
			g_fire(a_weapon, a_source, a_equipIndex, a_ammo, a_poison);
			t_shooter = outerShooter;
			t_shot = outerShot;

			if (worn) {
				WearByShot(*player, *worn, shot);
			}
		}

		// Registers the shot sink once. The event source lives as long as the
		// game, and a second sink would count every round twice.
		void RegisterShotSink()
		{
			static bool registered = false;
			if (registered) {
				return;
			}

			RE::WeaponFiredEvent::GetEventSource()->RegisterSink(new ShotSink());
			registered = true;
			REX::INFO("A gun wears by the power each shot leaves with, so a tap of a Gauss rifle costs less than a full charge.");
		}
	}

	void Install()
	{
		const REL::Relocation<std::uintptr_t> fire{ RE::ID::TESObjectWEAP::Fire };
		if (CallPatch::PatchCall(FIRE_SITE, fire.address(), reinterpret_cast<std::uintptr_t>(&FireHk), g_fire)) {
			REX::INFO("Guns wear down with every shot the game fires.");
		} else {
			REX::ERROR("Guns will not wear down when fired.");
		}
	}

	void Load()
	{
		RegisterHitSink();
		RegisterShotSink();
	}

	const RE::TESObjectREFR* Shooter()
	{
		return t_shooter;
	}

	bool FireLive()
	{
		return g_fire.Live();
	}
}
