#include "Gameplay/FireRate/Cuts.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace FireRate
{
	namespace
	{
		// The blow HitFrameHandler queues for each HitFrame event of an attack
		// animation. A held attack like the Ripper's sends one every 0.2 s from
		// a clip that plays at the same pace whatever the speed, so the pace is
		// kept here, see CutHk.
		constexpr CallPatch::CallSite CUT_SITE{ 2235282, 0x3E, "blade cut" };

		CallPatch::Link<void(RE::Actor*, std::uint32_t, bool)> g_cutLink;

		// How far each actor's worn blade is towards its next cut, by form ID.
		// Every HitFrame adds the blade's share and a cut costs 1, so a blade
		// at 0.75 lands 3 cuts in 4. HitFrames come from whichever thread
		// updates the actor, hence the lock.
		std::mutex                               g_cutLock;
		std::unordered_map<RE::TESFormID, float> g_cutCredits;

		// Whether the attack an actor is in runs on while the button is held,
		// as a blade cuts, the Ripper's and the minigun's Shredder bash. A bash
		// or a power attack with the Ripper does not, and lands whole.
		bool IsHeldAttack(const RE::Actor& a_actor)
		{
			const auto* process = a_actor.currentProcess;
			const auto* high = process ? process->high : nullptr;
			const auto* attack = high ? high->attackData.get() : nullptr;
			return attack && attack->data.flags.any(RE::AttackData::Flag::kContinuousAttack);
		}

		// Whether the next HitFrame of an actor's blade at this share lands,
		// see g_cutCredits. The credit carries from one press to the next, so
		// the long run is the share exactly.
		bool CutLands(RE::TESFormID a_actor, float a_share)
		{
			const std::scoped_lock l{ g_cutLock };
			auto&                  credit = g_cutCredits[a_actor];
			credit += a_share;
			if (credit < 1.0F) {
				return false;
			}
			credit -= 1.0F;
			return true;
		}

		// Whether the clip of a held attack already plays at the weapon speed,
		// see SpeedHk in FireRate.cpp. No melee clip does. The minigun's
		// Shredder bash does in the 3rd person graph every NPC uses, and so for
		// the player everywhere but in first person. Aiming keeps the first
		// person camera, the game never enters its iron sights camera.
		bool PlaysAtSpeed(const RE::TESObjectWEAP& a_weapon, bool a_player)
		{
			if (a_weapon.IsMeleeWeapon()) {
				return false;
			}
			const auto* camera = a_player ? RE::PlayerCamera::GetSingleton() : nullptr;
			return !camera || !camera->QCameraEquals(RE::CameraState::kFirstPerson);
		}

		// Whether the blow of this HitFrame lands. Only the held attack of a
		// worn automatic weapon whose clip ignores the speed is paced, see
		// PlaysAtSpeed, so a bash or a power attack with the Ripper and every
		// other blow land as ever. VATS keeps its own count of cuts, as it
		// keeps a burst's shots.
		bool Cuts(RE::Actor& a_actor, std::uint32_t a_equipIndex)
		{
			if (!IsHeldAttack(a_actor)) {
				return true;
			}

			RE::BGSObjectInstance item{ nullptr, nullptr };
			a_actor.GetEquippedItem(&item, RE::BGSEquipIndex{ a_equipIndex });
			const auto* object = item.object;
			if (!object || !object->IsWeapon()) {
				return true;
			}

			const auto& weapon = static_cast<const RE::TESObjectWEAP&>(*object);
			const auto* data = static_cast<const RE::TESObjectWEAP::InstanceData*>(item.instanceData.get());
			const bool  player = &a_actor == RE::PlayerCharacter::GetSingleton();
			if (!IsAutomatic(weapon, data) || PlaysAtSpeed(weapon, player)) {
				return true;
			}

			if (player) {
				const auto* vats = RE::VATS::GetSingleton();
				if (vats && vats->mode == RE::VATS::VATS_MODE_ENUM::kPlayback) {
					return true;
				}
			}

			// The share SpeedHk hands the animation.
			const auto share = player ? ShareOf(weapon) : NpcShareOf(a_actor, weapon);
			if (share >= 1.0F) {
				return true;
			}

			const auto lands = CutLands(a_actor.formID, share);
			TraceLog::For(!player).Line("blade cut", "{:s} with {:s}  x {:.4f}  {:s}",
				TraceLog::Who{ &a_actor }, TraceLog::Who{ &weapon }, share, lands ? "lands" : "skipped");
			return lands;
		}

		// Stands in for Actor::QueueMeleeHit where HitFrameHandler queues the
		// blow of a HitFrame. A worn blade lets through only its share of them,
		// the way a worn gun's animation fires only its share of the shots.
		void CutHk(RE::Actor* a_actor, std::uint32_t a_equipIndex, bool a_deal)
		{
			if (!Settings::bFireRate.GetValue() || !g_cutLink.Live() || Cuts(*a_actor, a_equipIndex)) {
				g_cutLink(a_actor, a_equipIndex, a_deal);
			}
		}
	}

	bool InstallCuts()
	{
		return CallPatch::PatchCall(CUT_SITE, RE::ID::Actor::QueueMeleeHit.address(), reinterpret_cast<std::uintptr_t>(&CutHk), g_cutLink);
	}

	void ForgetCuts()
	{
		const std::scoped_lock l{ g_cutLock };
		g_cutCredits.clear();
	}
}
