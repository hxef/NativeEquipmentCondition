#include "Gameplay/Aim/Aim.h"

#include "Condition/Condition.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/InHand.h"
#include "Gameplay/WeaponEvents/WeaponEvents.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <string_view>

namespace Aim
{
	namespace
	{
		// -------------------------------------------------------------------
		// Where the game asks for a cone
		// -------------------------------------------------------------------

		// The 1 call to AimModel::GetMinMaxCone inside ApplyConeOfFire, which
		// Fire runs once per shot before it sends the pellets. Skipped in VATS.
		constexpr CallPatch::CallSite SPREAD_SITE{ RE::ID::AimModel::ApplyConeOfFire.id(), 0xA4, "spread" };

		// The crosshair's min and max, in HUDCrosshair::UpdateComponent, and
		// its size right now, in GetCurrentCone, which only the crosshair
		// calls. Both ask for the player's aim model.
		constexpr CallPatch::CallSite CROSSHAIR_SITES[]{
			{ RE::ID::HUDCrosshair::UpdateComponent.id(), 0x236, "crosshair" },
			{ RE::ID::AimModel::GetCurrentCone.id(), 0x23, "crosshair size" },
		};

		// The 1 call to CombatFormulas::CalcWeaponAccuracy inside the VATS hit
		// chance, which only the VATS menu works out, for the player's weapon.
		constexpr CallPatch::CallSite VATS_SITE{ RE::ID::CombatFormulas::CalcVATSHitChance.id(), 0x85, "VATS accuracy" };

		using Cone_t = void (*)(const RE::AimModel*, float*, float*);
		using Accuracy_t = float (*)(const RE::BGSObjectInstanceT<RE::TESObjectWEAP>&, RE::Actor*);

		CallPatch::Link<Cone_t>                                         g_spreadLink;
		CallPatch::Held                                                 g_crosshair;
		std::array<CallPatch::Link<Cone_t>, std::size(CROSSHAIR_SITES)> g_crosshairLinks;
		CallPatch::Link<Accuracy_t>                                     g_vatsLink;

		// -------------------------------------------------------------------
		// The share and the caps
		// -------------------------------------------------------------------

		// The least accuracy a worn gun keeps. Below it a cone would grow
		// without end.
		constexpr float MIN_SHARE = 0.1F;

		// fAccuracyFloor, kept between MIN_SHARE and 1 the way FireRate.cpp
		// keeps fFireRateFloor.
		float Floor()
		{
			const auto floor = Settings::fAccuracyFloor.GetValue();
			return floor >= MIN_SHARE ? std::min(floor, 1.0F) : MIN_SHARE;
		}

		float Share(float a_health)
		{
			return Condition::Share(a_health, Floor(), Condition::Kind::kWeapon);
		}

		// Whether wear lowers accuracy at all.
		bool Lowers()
		{
			return Floor() < 1.0F;
		}

		// The game caps a cone in degrees, then turns it into radians with
		// this same float, 0.017453292.
		constexpr float RADIANS_PER_DEGREE = std::numbers::pi_v<float> / 180.0F;
		static_assert(RADIANS_PER_DEGREE == 0.017453292F);

		// fAimModelFireConeAngleLimitMax and Min, 30 and 20 degrees in an
		// unmodded game, found as game data loads.
		std::atomic<const RE::Setting*> g_maxLimit{ nullptr };
		std::atomic<const RE::Setting*> g_minLimit{ nullptr };

		// A cap in radians, as the game works it out.
		float Cap(const RE::Setting& a_limit)
		{
			return a_limit.GetFloat() * RADIANS_PER_DEGREE;
		}

		// a_cone widened by 1 over a_share and never past a_cap. A cone of 0
		// or less stays as the game gave it: an NPC's aim can take its cone
		// there, and a Courser's perk keeps it at 0 while it aims true.
		float Widen(float a_cone, float a_share, float a_cap)
		{
			if (!(a_cone > 0.0F) || a_share >= 1.0F) {
				return a_cone;
			}
			return std::max(a_cone, std::min(a_cone / a_share, a_cap));
		}

		// Widens a pair the game just worked out, the max into a_max and the
		// min into a_min. False while the caps are not found, which leaves the
		// pair as it is.
		bool WidenPair(float a_share, float* a_max, float* a_min)
		{
			const auto* maxLimit = g_maxLimit.load(std::memory_order_relaxed);
			const auto* minLimit = g_minLimit.load(std::memory_order_relaxed);
			if (!maxLimit || !minLimit) {
				return false;
			}
			*a_max = Widen(*a_max, a_share, Cap(*maxLimit));
			*a_min = Widen(*a_min, a_share, Cap(*minLimit));
			return true;
		}

		float Degrees(float a_radians)
		{
			return a_radians / RADIANS_PER_DEGREE;
		}

		// -------------------------------------------------------------------
		// The shot's spread
		// -------------------------------------------------------------------

		// The shot Fire is sending on this thread, see Fired.
		thread_local const Fired* t_fired = nullptr;

		// Stands in for AimModel::GetMinMaxCone where a shot spreads, for
		// every actor. The aim model names its actor, which has to be the one
		// the fire call noted.
		void SpreadHk(const RE::AimModel* a_aim, float* a_max, float* a_min)
		{
			g_spreadLink(a_aim, a_max, a_min);

			const auto* shot = t_fired;
			if (!shot || !g_spreadLink.Live() || !Lowers() || !a_aim || !a_max || !a_min ||
				static_cast<const RE::TESObjectREFR*>(a_aim->actor) != shot->shooter) {
				return;
			}

			const auto share = Share(shot->health);
			const auto max = *a_max;
			const auto min = *a_min;
			const auto widened = share < 1.0F && WidenPair(share, a_max, a_min);

			if (!TraceLog::IsOpen()) {
				return;
			}
			const auto tail = !(max > 0.0F) ? "  a cone of 0 or less stays"sv : (share < 1.0F && !widened ? "  no caps found"sv : ""sv);
			if (shot->shooter == RE::PlayerCharacter::GetSingleton()) {
				TraceLog::Line("aim", "{:s}  health {:.6f}  share {:.4f}  max {:.2f} -> {:.2f}  min {:.2f} -> {:.2f} degrees{:s}",
					TraceLog::Who{ shot->weapon }, shot->health, share, Degrees(max), Degrees(*a_max), Degrees(min), Degrees(*a_min), tail);
			} else {
				TraceLog::Npc::Line("aim", "{:s} with {:s}  health {:.6f}  share {:.4f}  max {:.2f} -> {:.2f}  min {:.2f} -> {:.2f} degrees{:s}",
					TraceLog::Who{ shot->shooter }, TraceLog::Who{ shot->weapon }, shot->health, share, Degrees(max), Degrees(*a_max),
					Degrees(min), Degrees(*a_min), tail);
			}
		}

		// -------------------------------------------------------------------
		// The crosshair and VATS
		// -------------------------------------------------------------------

		// The crosshair's gun and share the trace log last named, so a line
		// is written only when either changes.
		struct Said
		{
			RE::TESFormID weapon{ 0 };
			float         share{ 1.0F };
		};

		std::atomic<Said> g_crosshairSaid{ Said{} };

		// Stands in for AimModel::GetMinMaxCone at the 2 crosshair calls, I
		// being 0 for its min and max and 1 for its size right now. The gun is
		// the one the player holds this frame, see InHand.h.
		template <std::size_t I>
		void CrosshairHk(const RE::AimModel* a_aim, float* a_max, float* a_min)
		{
			g_crosshairLinks[I](a_aim, a_max, a_min);

			// The set is asked first, so a pair that waits still sees this
			// hook run while the crosshair has nothing to widen. It shows the
			// spread, so it widens only while the spread does.
			if (!g_crosshair.Runs(g_crosshairLinks[I]) || !SpreadsNow() || !a_aim || !a_max || !a_min ||
				static_cast<const RE::TESObjectREFR*>(a_aim->actor) != RE::PlayerCharacter::GetSingleton()) {
				return;
			}

			const auto  now = InHand::Now();
			const auto* weapon = now.weapon ? RE::TESForm::GetFormByID<RE::TESObjectWEAP>(now.weapon) : nullptr;
			const auto  share = weapon ? AccuracyShare(*weapon, now.health) : 1.0F;
			const auto  max = *a_max;
			const auto  min = *a_min;
			if (share < 1.0F) {
				WidenPair(share, a_max, a_min);
			}

			if constexpr (I == 0) {
				const Said said{ now.weapon, share };
				const auto before = g_crosshairSaid.exchange(said);
				if (weapon && (before.weapon != said.weapon || before.share != said.share)) {
					TraceLog::Line("aim", "crosshair  {:s}  health {:.6f}  share {:.4f}  max {:.2f} -> {:.2f}  min {:.2f} -> {:.2f} degrees",
						TraceLog::Who{ weapon }, now.health, share, Degrees(max), Degrees(*a_max), Degrees(min), Degrees(*a_min));
				}
			}
		}

		// Stands in for CombatFormulas::CalcWeaponAccuracy where VATS works
		// out its hit chance. Only a gun's term is scaled: for a melee weapon
		// VATS takes this term as its whole chance within reach. VATS never
		// spreads a shot, so this works whoever has the spread.
		float VatsAccuracyHk(const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, RE::Actor* a_perkOwner)
		{
			const auto  accuracy = g_vatsLink(a_weapon, a_perkOwner);
			const auto* object = a_weapon.object;
			if (!g_vatsLink.Live() || !Lowers() || !object || !object->IsWeapon()) {
				return accuracy;
			}

			// The weapon is the one the player holds.
			const auto& weapon = static_cast<const RE::TESObjectWEAP&>(*object);
			if (!weapon.IsGunWeapon() || !Condition::WearsOut(weapon)) {
				return accuracy;
			}
			const auto health = InHand::Health(weapon);
			const auto share = Share(health);

			// VATS asks every frame while it is open, hence Once.
			TraceLog::Once("vats", "{:s}  health {:.6f}  accuracy {:.4f} x {:.4f} = {:.4f}",
				TraceLog::Who{ &weapon }, health, accuracy, share, accuracy * share);
			return accuracy * share;
		}
	}

	void InstallAccuracy()
	{
		// The spread, the crosshair and VATS each go alone. The crosshair's 2
		// calls go in together, so its size never leaves its min and max
		// behind.
		const auto cone = RE::ID::AimModel::GetMinMaxCone.address();
		const auto spread = CallPatch::PatchCall(SPREAD_SITE, cone, reinterpret_cast<std::uintptr_t>(&SpreadHk), g_spreadLink);
		g_crosshair = CallPatch::PatchTogether({
			{ CROSSHAIR_SITES[0], cone, reinterpret_cast<std::uintptr_t>(&CrosshairHk<0>), &g_crosshairLinks[0] },
			{ CROSSHAIR_SITES[1], cone, reinterpret_cast<std::uintptr_t>(&CrosshairHk<1>), &g_crosshairLinks[1] },
		}, Part::kCrosshair);
		const auto vats = CallPatch::PatchCall(VATS_SITE, RE::ID::CombatFormulas::CalcWeaponAccuracy.address(),
			reinterpret_cast<std::uintptr_t>(&VatsAccuracyHk), g_vatsLink, Part::kVatsAccuracy);

		// Every shot's condition comes through the fire call, whose row has
		// already run, see Spreads.
		const auto fired = WeaponEvents::FireLive();
		if (spread && fired) {
			REX::INFO("A worn gun spreads its shots wider, for anyone, down to {:.2f} of its accuracy at 0 condition.", Floor());
		} else {
			REX::WARN("A worn gun will keep the spread of a new one.");
		}
		if (!g_crosshair || !spread || !fired) {
			REX::WARN("The crosshair will keep the size of a new gun's.");
		}
		if (!vats) {
			REX::WARN("VATS will keep counting a worn gun at the accuracy of a new one.");
		}
	}

	void LoadAccuracy()
	{
		auto*       settings = RE::GameSettingCollection::GetSingleton();
		const auto* maxLimit = settings ? settings->GetSetting("fAimModelFireConeAngleLimitMax"sv) : nullptr;
		const auto* minLimit = settings ? settings->GetSetting("fAimModelFireConeAngleLimitMin"sv) : nullptr;
		const auto  floats = maxLimit && minLimit && maxLimit->GetType() == RE::Setting::SETTING_TYPE::kFloat &&
		                    minLimit->GetType() == RE::Setting::SETTING_TYPE::kFloat;
		g_maxLimit = floats ? maxLimit : nullptr;
		g_minLimit = floats ? minLimit : nullptr;
		if (!floats) {
			REX::ERROR("fAimModelFireConeAngleLimitMax or Min is missing, so a worn gun keeps the spread of a new one.");
			return;
		}
		REX::INFO("No worn gun spreads past the game's own limits, {:.1f} degrees for the widest cone and {:.1f} for the narrowest.",
			maxLimit->GetFloat(), minLimit->GetFloat());
	}

	bool Spreads()
	{
		return g_spreadLink.Live() && WeaponEvents::FireLive();
	}

	bool SpreadsNow()
	{
		return Spreads() && Lowers();
	}

	float AccuracyShare(const RE::TESObjectWEAP& a_weapon, float a_health)
	{
		return SpreadsNow() && a_weapon.IsGunWeapon() ? Share(a_health) : 1.0F;
	}

	Fired::Fired(const RE::TESObjectREFR* a_shooter, const RE::TESObjectWEAP& a_weapon, float a_health) :
		shooter(a_shooter), weapon(&a_weapon), health(a_health), outer(t_fired)
	{
		t_fired = this;
	}

	Fired::~Fired()
	{
		t_fired = outer;
	}
}
