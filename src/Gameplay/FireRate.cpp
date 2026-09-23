#include "Gameplay/FireRate.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Core/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/WeaponEvents/WeaponEvents.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

namespace FireRate
{
	namespace
	{
		// The 3 calls the pace of an automatic weapon is read through. The
		// weapon speed the animation graph is given every frame,
		// weaponSpeedMult for the first equip slot and leftWeaponSpeedMult for
		// the second.
		constexpr CallPatch::CallSite SPEED_SITE{ 2235244, 0x32, "fire speed" };

		// The rate the player's attack handler builds its countdown from. Only
		// a weapon with an attack delay loads it, which leaves out the minigun,
		// and a bolt action takes a branch of its own.
		constexpr CallPatch::CallSite RATE_SITE{ 2234929, 0x140, "fire rate" };

		// The rate the automatic weapon sound picks its loop by, inside the
		// engine's Fire, as a burst starts.
		constexpr CallPatch::CallSite SOUND_SITE{ 2196901, 0x2F, "fire sound" };

		// The blow HitFrameHandler queues for each HitFrame event of an attack
		// animation. A held attack like the Ripper's sends one every 0.2 s from
		// a clip that plays at the same pace whatever the speed, so the pace is
		// kept here, see CutHk.
		constexpr CallPatch::CallSite CUT_SITE{ 2235282, 0x3E, "blade cut" };

		// The slowest a worn gun's animation plays. A gun at 0 would never get
		// through its attack.
		constexpr float MIN_SHARE = 0.1F;

		// fFireRateFloor, kept between MIN_SHARE and 1. A value that is not a
		// number counts as MIN_SHARE.
		float Floor()
		{
			const auto floor = Settings::fFireRateFloor.GetValue();
			return floor >= MIN_SHARE ? std::min(floor, 1.0F) : MIN_SHARE;
		}

		// The share of its speed a gun at this condition is played at, the line
		// of HealthDamage/Curve.h.
		float Share(float a_health)
		{
			return Condition::Share(a_health, Floor());
		}

		// A share and the weapon it was read for.
		struct Reading
		{
			RE::TESFormID weapon{ 0 };
			float         share{ 1.0F };
		};

		// The share for the gun in the player's hands. A Reading is 8 bytes, so
		// one atomic holds it whole.
		std::atomic<Reading> g_reading{ Reading{} };

		// Set while a measurement waits in the task queue.
		std::atomic<bool> g_queued{ false };

		// The share of every NPC whose automatic weapon is worn, by form ID,
		// and the weapon it was read for. No entry means full rate. The
		// animation reads these from whichever thread updates it, hence the
		// lock.
		std::shared_mutex                          g_npcLock;
		std::unordered_map<RE::TESFormID, Reading> g_npcReadings;

		// How far each actor's worn blade is towards its next cut, by form ID.
		// Every HitFrame adds the blade's share and a cut costs 1, so a blade
		// at 0.75 lands 3 cuts in 4. HitFrames come from whichever thread
		// updates the actor, hence the lock.
		std::mutex                               g_cutLock;
		std::unordered_map<RE::TESFormID, float> g_cutCredits;

		// Whether the animation's call took. Written once while the plugin
		// loads.
		bool g_patched = false;

		// Measures the share for the player's equipped copy. The weapon arrives
		// as a form ID and is looked up again, so a form the game has freed is
		// never touched. While another thread holds the inventory lock, the
		// last share is kept for a frame.
		void Measure(RE::TESFormID a_weapon)
		{
			auto*       player = RE::PlayerCharacter::GetSingleton();
			const auto* weapon = RE::TESForm::GetFormByID(a_weapon);
			const auto  health = player && weapon ?
			                         Equipped::TryWeaponHealth(player, weapon) :
			                         std::optional{ Condition::INVALID_HEALTH };
			if (!health) {
				TraceLog::Once("fire speed", "{:s} [{:08X}]  kept its last share, another thread had the player's inventory",
					TraceLog::Who{ weapon }, a_weapon);
				return;
			}

			const Reading now{ a_weapon, Share(*health) };
			const auto    before = g_reading.exchange(now);
			if (before.weapon != now.weapon || before.share != now.share) {
				TraceLog::Line("fire speed", "{:s} [{:08X}]  health {:.6f}  played at x {:.4f}",
					TraceLog::Who{ weapon }, a_weapon, *health, now.share);
			}
		}

		// Asks for the share to be measured again, as HudParts.cpp asks for the
		// HUD's condition. At most one waits at a time.
		void Queue(const RE::TESObjectWEAP& a_weapon)
		{
			if (g_queued.exchange(true)) {
				return;
			}

			const auto* tasks = F4SE::GetTaskInterface();
			if (!tasks) {
				g_queued = false;
				return;
			}

			// F4SE runs queued tasks a frame later, on a worker thread during
			// play.
			tasks->AddTask([id = a_weapon.formID] {
				Measure(id);
				g_queued = false;
			});
		}

		// The share for a_weapon, or 1 when the last measurement was for
		// another gun, the case for a frame after a swap.
		float ShareOf(const RE::TESObjectWEAP& a_weapon)
		{
			const auto reading = g_reading.load();
			return reading.weapon == a_weapon.formID ? reading.share : 1.0F;
		}

		// The share for the weapon an NPC holds, or 1 when it has not used that
		// weapon since.
		float NpcShareOf(const RE::TESForm& a_actor, const RE::TESObjectWEAP& a_weapon)
		{
			const std::shared_lock l{ g_npcLock };
			const auto             it = g_npcReadings.find(a_actor.formID);
			return it != g_npcReadings.end() && it->second.weapon == a_weapon.formID ? it->second.share : 1.0F;
		}

		// Whether this copy keeps attacking while the trigger is held: an
		// automatic gun or a motor driven blade. The copy's own data where it
		// has any, since a receiver is what makes most guns automatic.
		bool IsAutomatic(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data)
		{
			const auto& stats = a_data ? *a_data : a_weapon.weaponData;
			return stats.flags.any(RE::WEAPON_FLAGS::kAutomatic);
		}

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

		// Stands in for CombatFormulas::CalcWeaponSpeedMult where the animation
		// graph is given its weaponSpeedMult. The equip slot arrives as a plain
		// number.
		float SpeedHk(const RE::Actor* a_actor, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, std::uint32_t a_equipIndex)
		{
			const auto speed = RE::CombatFormulas::CalcWeaponSpeedMult(a_actor, a_weapon, RE::BGSEquipIndex{ a_equipIndex });

			// Both slots of every actor are asked every frame, so anything but
			// an automatic weapon in the first slot returns at once.
			const auto* object = a_weapon.object;
			if (a_equipIndex != 0 || !a_actor || !object || !object->IsWeapon()) {
				return speed;
			}

			const auto& weapon = static_cast<const RE::TESObjectWEAP&>(*object);
			const auto* data = static_cast<const RE::TESObjectWEAP::InstanceData*>(a_weapon.instanceData.get());
			if (!IsAutomatic(weapon, data)) {
				return speed;
			}

			// An NPC's share was read as it last used the weapon, see
			// NoteNpcWeapon.
			if (a_actor != RE::PlayerCharacter::GetSingleton()) {
				return speed * NpcShareOf(*a_actor, weapon);
			}

			// The player's is measured for the next frame and read from the
			// last.
			Queue(weapon);
			return speed * ShareOf(weapon);
		}

		// Stands in for TESObjectWEAP::GetRateOfFire in the attack handler's
		// countdown, so the wait between 2 presses keeps pace with the burst.
		// The handler and the gun are the player's.
		float CountdownHk(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data)
		{
			const auto rate = RE::TESObjectWEAP::GetRateOfFire(a_weapon, a_data);
			if (rate <= 0.0F || !IsAutomatic(a_weapon, a_data)) {
				return rate;
			}

			const auto share = ShareOf(a_weapon);
			if (share != 1.0F) {
				TraceLog::Line("fire rate", "{:s} [{:08X}]  {:.2f} x {:.4f} = {:.2f} attacks a second",
					TraceLog::Who{ &a_weapon }, a_weapon.formID, rate, share, rate * share);
			}
			return rate * share;
		}

		// Stands in for TESObjectWEAP::GetRateOfFire where the automatic weapon
		// sound starts. The sound picks the loop nearest to the rate, so a
		// slower burst gets a slower loop where the gun has one.
		float SoundHk(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data)
		{
			const auto rate = RE::TESObjectWEAP::GetRateOfFire(a_weapon, a_data);

			// Every actor's shots start their sound here, and WeaponEvents says
			// whose shot is on its way through Fire.
			const auto* shooter = WeaponEvents::Shooter();
			if (rate <= 0.0F || !shooter || !IsAutomatic(a_weapon, a_data)) {
				return rate;
			}

			if (shooter == RE::PlayerCharacter::GetSingleton()) {
				const auto share = ShareOf(a_weapon);
				if (share != 1.0F) {
					TraceLog::Line("fire sound", "{:s} [{:08X}]  {:.0f} x {:.4f} = {:.0f} shots a minute",
						TraceLog::Who{ &a_weapon }, a_weapon.formID, rate * 60.0F, share, rate * share * 60.0F);
				}
				return rate * share;
			}

			// An NPC's share was read a moment ago as the shot started, see
			// NoteNpcWeapon.
			const auto share = NpcShareOf(*shooter, a_weapon);
			if (share != 1.0F) {
				TraceLog::Npc::Line("fire sound", "{:s} with {:s}  {:.0f} x {:.4f} = {:.0f} shots a minute",
					TraceLog::Who{ shooter }, TraceLog::Who{ &a_weapon }, rate * 60.0F, share, rate * share * 60.0F);
			}
			return rate * share;
		}

		// Whether the clip of a held attack already plays at the weapon speed,
		// see SpeedHk. No melee clip does. The minigun's Shredder bash does in
		// the 3rd person graph every NPC uses, and so for the player everywhere
		// but in first person. Aiming keeps the first person camera, the game
		// never enters its iron sights camera.
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
			if (Cuts(*a_actor, a_equipIndex)) {
				a_actor->QueueMeleeHit(RE::BGSEquipIndex{ a_equipIndex }, a_deal);
			}
		}
	}

	void Install()
	{
		// The animation is what fires a burst. Without it the other 2 would
		// slow the presses and the sound of a gun firing as fast as ever, so
		// nothing is patched.
		if (!CallPatch::PatchCall(SPEED_SITE, RE::ID::CombatFormulas::CalcWeaponSpeedMult.address(),
				reinterpret_cast<std::uintptr_t>(&SpeedHk))) {
			REX::ERROR("A worn automatic weapon will keep firing at its full rate.");
			return;
		}
		g_patched = true;

		const auto rateOfFire = RE::ID::TESObjectWEAP::GetRateOfFire.address();
		if (!CallPatch::PatchCall(RATE_SITE, rateOfFire, reinterpret_cast<std::uintptr_t>(&CountdownHk))) {
			REX::ERROR("A worn automatic weapon will not wait any longer between two presses.");
		}
		if (!CallPatch::PatchCall(SOUND_SITE, rateOfFire, reinterpret_cast<std::uintptr_t>(&SoundHk))) {
			REX::ERROR("A worn automatic weapon will sound as fast as a new one.");
		}
		if (!CallPatch::PatchCall(CUT_SITE, RE::ID::Actor::QueueMeleeHit.address(), reinterpret_cast<std::uintptr_t>(&CutHk))) {
			REX::ERROR("A worn Ripper will keep cutting as often as a new one.");
		}

		REX::INFO("A worn automatic weapon fires slower, in anybody's hands, down to {:.2f} of its rate at nothing.", Floor());
	}

	void Unload()
	{
		{
			const std::unique_lock l{ g_npcLock };
			g_npcReadings.clear();
		}
		const std::scoped_lock l{ g_cutLock };
		g_cutCredits.clear();
	}

	bool Slows()
	{
		return g_patched;
	}

	float RateShare(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data, float a_health)
	{
		return Slows() && IsAutomatic(a_weapon, a_data) ? Share(a_health) : 1.0F;
	}

	void NoteNpcWeapon(RE::Actor& a_actor, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon)
	{
		const auto* object = a_weapon.object;
		if (!Slows() || !object || !object->IsWeapon()) {
			return;
		}

		const auto& weapon = static_cast<const RE::TESObjectWEAP&>(*object);
		const auto* data = static_cast<const RE::TESObjectWEAP::InstanceData*>(a_weapon.instanceData.get());
		if (!IsAutomatic(weapon, data) || !Condition::WearsOut(weapon)) {
			return;
		}

		const auto health = Equipped::TryWeaponHealth(&a_actor, &weapon);
		if (!health) {
			TraceLog::Npc::Line("fire speed", "{:s} with {:s}  kept its last share, its inventory was busy",
				TraceLog::Who{ &a_actor }, TraceLog::Who{ &weapon });
			return;
		}

		// No entry means full rate, so a weapon at full condition takes the
		// entry out.
		const Reading now{ weapon.formID, Share(*health) };
		auto          before = 1.0F;
		{
			const std::unique_lock l{ g_npcLock };
			const auto             it = g_npcReadings.find(a_actor.formID);
			if (it != g_npcReadings.end()) {
				if (it->second.weapon == now.weapon) {
					before = it->second.share;
				}
				if (now.share == 1.0F) {
					g_npcReadings.erase(it);
				} else {
					it->second = now;
				}
			} else if (now.share != 1.0F) {
				g_npcReadings.emplace(a_actor.formID, now);
			}
		}

		if (before != now.share) {
			TraceLog::Npc::Line("fire speed", "{:s} with {:s}  health {:.6f}  played at x {:.4f}",
				TraceLog::Who{ &a_actor }, TraceLog::Who{ &weapon }, *health, now.share);
		}
	}
}
