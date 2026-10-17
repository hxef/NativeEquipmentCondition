#include "Gameplay/FireRate/FireRate.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/FireRate/Cuts.h"
#include "Gameplay/InHand.h"
#include "Gameplay/WeaponEvents/WeaponEvents.h"

#include <algorithm>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

namespace FireRate
{
	namespace
	{
		// -------------------------------------------------------------------
		// Where the game asks for a pace
		// -------------------------------------------------------------------

		// The 3 calls the rate of an automatic weapon is read through. The
		// sound is a smaller part, see Install. The weapon speed the animation
		// graph is given every frame, weaponSpeedMult for the first equip slot
		// and leftWeaponSpeedMult for the second.
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

		// -------------------------------------------------------------------
		// The share and NPCs
		// -------------------------------------------------------------------

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
			return Condition::Share(a_health, Floor(), Condition::Kind::kWeapon);
		}

		// Whether fFireRateFloor slows a worn weapon.
		bool Lowers()
		{
			return Floor() < 1.0F;
		}

		// A share and the weapon it was read for.
		struct Reading
		{
			RE::TESFormID weapon{ 0 };
			float         share{ 1.0F };
		};

		// The share of every NPC whose automatic weapon is worn, by form ID,
		// and the weapon it was read for. No entry means full rate. The
		// animation reads these from whichever thread updates it, hence the
		// lock.
		std::shared_mutex                          g_npcLock;
		std::unordered_map<RE::TESFormID, Reading> g_npcReadings;

		// The weapon an NPC used when it keeps a pace NEC reads: an automatic
		// weapon that wears, while worn weapons slow, see SlowsNow. Null for
		// any other.
		const RE::TESObjectWEAP* Paced(const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon)
		{
			const auto* object = a_weapon.object;
			if (!SlowsNow() || !object || !object->IsWeapon()) {
				return nullptr;
			}
			const auto& weapon = static_cast<const RE::TESObjectWEAP&>(*object);
			const auto* data = static_cast<const RE::TESObjectWEAP::InstanceData*>(a_weapon.instanceData.get());
			return IsAutomatic(weapon, data) && Condition::WearsOut(weapon) ? &weapon : nullptr;
		}

		// Keeps an NPC's share from a_health, or its last share while the
		// health could not be read.
		void Keep(RE::Actor& a_actor, const RE::TESObjectWEAP& a_weapon, std::optional<float> a_health)
		{
			if (!a_health) {
				TraceLog::Npc::Line("fire speed", "{:s} with {:s}  kept its last share, its inventory was busy",
					TraceLog::Who{ &a_actor }, TraceLog::Who{ &a_weapon });
				return;
			}

			// No entry means full rate, so a weapon at its full rate takes the
			// entry out.
			const Reading now{ a_weapon.formID, Share(*a_health) };
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
					TraceLog::Who{ &a_actor }, TraceLog::Who{ &a_weapon }, *a_health, now.share);
			}
		}

		// -------------------------------------------------------------------
		// The hooks
		// -------------------------------------------------------------------

		// Stands in for CombatFormulas::CalcWeaponSpeedMult where the animation
		// graph is given its weaponSpeedMult. The equip slot arrives as a plain
		// number.
		float SpeedHk(const RE::Actor* a_actor, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, std::uint32_t a_equipIndex)
		{
			const auto speed = g_speedLink(a_actor, a_weapon, a_equipIndex);

			// Both slots of every actor are asked every frame, so anything but
			// an automatic weapon in the first slot returns at once.
			const auto* object = a_weapon.object;
			if (!SlowsNow() || a_equipIndex != 0 || !a_actor || !object || !object->IsWeapon()) {
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

			// The player's is read from this frame's measurement, see
			// InHand.h.
			return speed * ShareOf(weapon);
		}

		// Stands in for TESObjectWEAP::GetRateOfFire in the attack handler's
		// countdown, so the wait between 2 presses keeps pace with the burst.
		// The handler and the gun are the player's.
		float CountdownHk(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data)
		{
			const auto rate = g_countdownLink(a_weapon, a_data);
			if (!SlowsNow() || rate <= 0.0F || !IsAutomatic(a_weapon, a_data)) {
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
			// whose shot is on its way through Fire. The sound stands alone
			// and follows the pace.
			const auto* shooter = WeaponEvents::Shooter();
			if (!g_soundLink.Live() || !SlowsNow() || rate <= 0.0F || !shooter || !IsAutomatic(a_weapon, a_data)) {
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
		// The animation is what fires a burst, and the pace needs the speed,
		// countdown and cut places, see Slows. Each stands alone, so a mod
		// that still hands the call on to NEC shares its place and the pace
		// keeps working.
		const auto rateOfFire = RE::ID::TESObjectWEAP::GetRateOfFire.address();
		const auto speed = CallPatch::PatchCall(SPEED_SITE, RE::ID::CombatFormulas::CalcWeaponSpeedMult.address(),
			reinterpret_cast<std::uintptr_t>(&SpeedHk), g_speedLink);
		const auto rate = CallPatch::PatchCall(RATE_SITE, rateOfFire, reinterpret_cast<std::uintptr_t>(&CountdownHk), g_countdownLink);
		const auto cuts = InstallCuts();
		// A smaller part: without it a worn gun still fires slower and sounds
		// as fast as a new one.
		const auto sound = CallPatch::PatchCall(SOUND_SITE, rateOfFire, reinterpret_cast<std::uintptr_t>(&SoundHk), g_soundLink, Part::kFireSound);
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
		{
			const std::unique_lock l{ g_npcLock };
			g_npcReadings.clear();
		}
		ForgetCuts();
	}

	bool Slows()
	{
		return g_speedLink.Live() && g_countdownLink.Live() && CutLive();
	}

	bool SlowsNow()
	{
		return Slows() && Lowers();
	}

	float RateShare(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data, float a_health)
	{
		return SlowsNow() && IsAutomatic(a_weapon, a_data) ? Share(a_health) : 1.0F;
	}

	void NoteNpcWeapon(RE::Actor& a_actor, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon)
	{
		const auto* weapon = Paced(a_weapon);
		if (weapon) {
			Keep(a_actor, *weapon, Equipped::TryWeaponHealth(&a_actor, weapon));
		}
	}

	void NoteNpcWeapon(RE::Actor& a_actor, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, std::optional<float> a_health)
	{
		if (const auto* weapon = Paced(a_weapon)) {
			Keep(a_actor, *weapon, a_health);
		}
	}

	bool IsAutomatic(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_data)
	{
		const auto& stats = a_data ? *a_data : a_weapon.weaponData;
		return stats.flags.any(RE::WEAPON_FLAGS::kAutomatic);
	}

	float ShareOf(const RE::TESObjectWEAP& a_weapon)
	{
		return Share(InHand::Health(a_weapon));
	}

	float NpcShareOf(const RE::TESForm& a_actor, const RE::TESObjectWEAP& a_weapon)
	{
		const std::shared_lock l{ g_npcLock };
		const auto             it = g_npcReadings.find(a_actor.formID);
		return it != g_npcReadings.end() && it->second.weapon == a_weapon.formID ? it->second.share : 1.0F;
	}
}
