#include "Gameplay/FireRate/FireRate.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/FireRate/Cuts.h"
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
		// engine's Fire, as a burst starts. A burst starts up to 2 sounds, the
		// shots and one for the surroundings, and each reads the rate here.
		constexpr CallPatch::CallSite SOUND_SITE{ 2196901, 0x2F, "fire sound" };

		CallPatch::Link<float(const RE::Actor*, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>&, std::uint32_t)> g_speedLink;
		CallPatch::Link<float(const RE::TESObjectWEAP&, const RE::TESObjectWEAP::InstanceData*)>                  g_countdownLink;
		CallPatch::Link<float(const RE::TESObjectWEAP&, const RE::TESObjectWEAP::InstanceData*)>                  g_soundLink;

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
				TraceLog::Once("fire speed", "{:s}  kept its last share, another thread had the player's inventory",
					TraceLog::Who{ weapon });
				return;
			}

			// INVALID_HEALTH here means the player holds no copy of this gun
			// that wears, so it plays at full speed.
			const Reading now{ a_weapon, Share(*health) };
			const auto    before = g_reading.exchange(now);
			if (before.weapon == now.weapon && before.share == now.share) {
				return;
			}
			if (*health < 0.0F) {
				TraceLog::Line("fire speed", "{:s}  no copy that wears in hand  played at x {:.4f}", TraceLog::Who{ weapon }, now.share);
			} else {
				TraceLog::Line("fire speed", "{:s}  health {:.6f}  played at x {:.4f}",
					TraceLog::Who{ weapon }, *health, now.share);
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

		// Whether worn weapons slow right now. Asked on every call, so the
		// switch works while the game runs.
		bool On()
		{
			return Settings::bFireRate.GetValue();
		}

		// Stands in for CombatFormulas::CalcWeaponSpeedMult where the animation
		// graph is given its weaponSpeedMult. The equip slot arrives as a plain
		// number.
		float SpeedHk(const RE::Actor* a_actor, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, std::uint32_t a_equipIndex)
		{
			const auto speed = g_speedLink(a_actor, a_weapon, a_equipIndex);

			// Both slots of every actor are asked every frame, so anything but
			// an automatic weapon in the first slot returns at once.
			const auto* object = a_weapon.object;
			if (!On() || !g_speedLink.Live() || a_equipIndex != 0 || !a_actor || !object || !object->IsWeapon()) {
				return speed;
			}

			const auto& weapon = static_cast<const RE::TESObjectWEAP&>(*object);
			const auto* data = static_cast<const RE::TESObjectWEAP::InstanceData*>(a_weapon.instanceData.get());
			if (!IsAutomatic(weapon, data)) {
				return speed;
			}

			// An NPC's share was read as it last used the weapon, see
			// NoteNpcWeapon. A gun's is read by the fire call, so it counts
			// only while that call is NEC's.
			if (a_actor != RE::PlayerCharacter::GetSingleton()) {
				const auto gun = weapon.weaponData.type.get() == RE::WEAPON_TYPE::kGun;
				return gun && !WeaponEvents::FireLive() ? speed : speed * NpcShareOf(*a_actor, weapon);
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
			const auto rate = g_countdownLink(a_weapon, a_data);
			if (!On() || !g_countdownLink.Live() || rate <= 0.0F || !IsAutomatic(a_weapon, a_data)) {
				return rate;
			}

			const auto share = ShareOf(a_weapon);
			if (share != 1.0F) {
				TraceLog::Line("fire rate", "{:s}  {:.2f} x {:.4f} = {:.2f} attacks a second",
					TraceLog::Who{ &a_weapon }, rate, share, rate * share);
			}
			return rate * share;
		}

		// Stands in for TESObjectWEAP::GetRateOfFire where the automatic weapon
		// sound starts. The sound picks the loop nearest to the rate, so a
		// slower burst gets a slower loop where the gun has one. It runs up to
		// twice a burst, see SOUND_SITE.
		float SoundHk(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data)
		{
			const auto rate = g_soundLink(a_weapon, a_data);

			// Every actor's shots start their sound here, and WeaponEvents says
			// whose shot is on its way through Fire.
			const auto* shooter = WeaponEvents::Shooter();
			if (!On() || !g_soundLink.Live() || rate <= 0.0F || !shooter || !IsAutomatic(a_weapon, a_data)) {
				return rate;
			}

			if (shooter == RE::PlayerCharacter::GetSingleton()) {
				const auto share = ShareOf(a_weapon);
				if (share != 1.0F) {
					TraceLog::Line("fire sound", "{:s}  {:.0f} x {:.4f} = {:.0f} shots a minute",
						TraceLog::Who{ &a_weapon }, rate * 60.0F, share, rate * share * 60.0F);
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
	}

	void Install()
	{
		// Every place is noted before any is judged, so each mod that has one
		// is named. The animation is what fires a burst, and the 3 main places
		// work as one: with any of them left to another mod NEC writes none of
		// the row, see End in CallPatch.h.
		const auto rateOfFire = RE::ID::TESObjectWEAP::GetRateOfFire.address();
		const auto speed = CallPatch::PatchCall(SPEED_SITE, RE::ID::CombatFormulas::CalcWeaponSpeedMult.address(),
			reinterpret_cast<std::uintptr_t>(&SpeedHk), g_speedLink);
		const auto rate = CallPatch::PatchCall(RATE_SITE, rateOfFire, reinterpret_cast<std::uintptr_t>(&CountdownHk), g_countdownLink);
		// A smaller part: without it a worn gun still fires slower and sounds
		// as fast as a new one.
		const auto sound = CallPatch::PatchCall(SOUND_SITE, rateOfFire, reinterpret_cast<std::uintptr_t>(&SoundHk), g_soundLink, Part::kFireSound);
		const auto cuts = InstallCuts();
		if (!speed || !rate || !cuts) {
			REX::WARN("A worn automatic weapon will keep firing at its full rate.");
			return;
		}
		if (!sound) {
			REX::WARN("A worn automatic weapon will sound as fast as a new one.");
		}

		REX::INFO("A worn automatic weapon fires slower, for anyone, down to {:.2f} of its rate at 0 condition.", Floor());
	}

	void Unload()
	{
		g_reading = Reading{};
		{
			const std::unique_lock l{ g_npcLock };
			g_npcReadings.clear();
		}
		ForgetCuts();
	}

	bool Slows()
	{
		return !CallPatch::IsYielded(Settings::bFireRate);
	}

	float RateShare(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data, float a_health)
	{
		return On() && IsAutomatic(a_weapon, a_data) ? Share(a_health) : 1.0F;
	}

	void NoteNpcWeapon(RE::Actor& a_actor, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon)
	{
		const auto* object = a_weapon.object;
		if (!On() || !object || !object->IsWeapon()) {
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

	bool IsAutomatic(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data)
	{
		const auto& stats = a_data ? *a_data : a_weapon.weaponData;
		return stats.flags.any(RE::WEAPON_FLAGS::kAutomatic);
	}

	float ShareOf(const RE::TESObjectWEAP& a_weapon)
	{
		const auto reading = g_reading.load();
		return reading.weapon == a_weapon.formID ? reading.share : 1.0F;
	}

	float NpcShareOf(const RE::TESForm& a_actor, const RE::TESObjectWEAP& a_weapon)
	{
		const std::shared_lock l{ g_npcLock };
		const auto             it = g_npcReadings.find(a_actor.formID);
		return it != g_npcReadings.end() && it->second.weapon == a_weapon.formID ? it->second.share : 1.0F;
	}
}
