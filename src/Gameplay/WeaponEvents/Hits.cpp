#include "Gameplay/WeaponEvents/Hits.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Condition/WeaponWear/WeaponWear.h"
#include "Core/ItemCards.h"
#include "Core/TraceLog.h"
#include "Gameplay/FireRate/FireRate.h"

#include <string>
#include <string_view>
#include <utility>

namespace WeaponEvents
{
	namespace
	{
		// What a bash costs the weapon compared with one ordinary use. Fallout
		// 76 wears a gun swung as a club once per attack based on base damage,
		// and only for blows that land, which holds here since only landed
		// blows raise the event. Fallout 3 takes 3% of base damage for a gun
		// shot and 5% for a melee attack, so a blow by hand costs 1.67 shots,
		// and that is the ratio used.
		constexpr float BASH_WEAR = 5.0F / 3.0F;

		// Whether a blow was struck with the weapon rather than fired out of
		// it. HitData::Populate writes the melee flag first, ahead of every
		// branch, so the hit is asked and not the weapon: a weapon type says
		// what is held, not how it was used, and a rifle butt is a melee blow
		// with a gun.
		bool ByHand(const RE::HitData& a_hit)
		{
			return a_hit.flags.any(RE::HitData::Flag::kMeleeAttack);
		}

		// Whether a blow struck by hand was a bash. A gun bash is an ordinary
		// melee blow with a gun in hand, never flagged as a bash, and the melee
		// flag plus a gun is the pair the engine hands to the Basher perk. The
		// engine's own kBash kind reads the attacker's attack state and stops
		// filling the weapon in, with a short damage formula of its own.
		// Vanilla never hands the player that kind, the bash button power
		// attacks once a melee weapon is in hand, but combat AI reaches it and
		// a mod could.
		bool IsBash(const RE::HitData& a_hit, const RE::TESObjectWEAP& a_weapon)
		{
			return a_hit.flags.any(RE::HitData::Flag::kBash, RE::HitData::Flag::kTimedBash) ||
			       a_weapon.IsGunWeapon();
		}

		// How much harder than an ordinary blow the game made this one land.
		// Every blow comes through one of the race's attacks, each with a
		// damage multiplier HitData::Populate puts on the damage: 1.5 for a
		// human power attack, 2.0 in power armor, 1.0 for a swing and a gun
		// bash. A weapon wears in proportion to what it hits for, see
		// WeaponWear.h. The attack is asked and not the power attack flag,
		// which is on the gun bash too. No attack, or a multiplier that is not
		// positive, counts as ordinary.
		float AttackMult(const RE::HitData& a_hit)
		{
			const auto* attack = a_hit.attackData.get();
			const auto  mult = attack ? attack->data.damageMult : 1.0F;
			return mult > 0.0F ? mult : 1.0F;
		}

		// The flags worth reading on a hit line, in engine order. A melee
		// weapon bash reads "bash melee", a swing "melee", a power attack
		// "power melee", a round "shot". A gun bash reads "power melee" too,
		// since the race files it among the power attacks. The raw value goes
		// beside the names, so an unnamed flag still shows.
		std::string Marks(const RE::HitData& a_hit)
		{
			using Flag = RE::HitData::Flag;
			constexpr std::pair<Flag, std::string_view> NAMED[]{
				{ Flag::kSneakAttack, "sneak"sv },
				{ Flag::kBash, "bash"sv },
				{ Flag::kTimedBash, "timed bash"sv },
				{ Flag::kPowerAttack, "power"sv },
				{ Flag::kMeleeAttack, "melee"sv },
			};

			std::string marks;
			for (const auto& [flag, name] : NAMED) {
				if (a_hit.flags.any(flag)) {
					if (!marks.empty()) {
						marks += ' ';
					}
					marks += name;
				}
			}
			return marks.empty() ? "shot" : marks;
		}

		// Prints what a hit cost the target, the only way to tell whether the
		// penalty reached combat. totalDamage is everything before armor,
		// resistedPhysicalDamage what armor took off, physicalDamage what got
		// through, healthDamage what the target lost. Total is the one to
		// compare while testing.
		void LogPlayerHit(const RE::HitData& a_hit, const RE::TESForm* a_weapon, bool a_byHand, bool a_bash)
		{
			// The event's own target and cause are empty whenever it carries
			// hit data. The hit data names the target.
			const auto  target = a_hit.target.get();
			const char* targetName = target ? target->GetDisplayFullName() : nullptr;

			// A punch is thrown with UnarmedHuman [000C2C27], which has no
			// name, so a nameless weapon behind a blow by hand is the player's
			// fists.
			auto weaponName = a_weapon ? RE::TESFullName::GetFullName(*a_weapon) : ""sv;
			if (weaponName.empty()) {
				weaponName = a_byHand ? "fists"sv : "?"sv;
			}
			const auto weaponID = a_weapon ? a_weapon->formID : 0;

			// A blow by hand is the first event the plugin gets for the swing,
			// so it opens a block. A round joins the block its shot opened.
			if (a_byHand) {
				TraceLog::Begin(a_bash ? "BASH"sv : "MELEE"sv, "{:s} [{:08X}] on {:s}",
					weaponName, weaponID, targetName ? targetName : "?");
			}

			TraceLog::Line("hit", "{:s} with {:s} [{:08X}]  physical {:.2f}  resisted {:.2f}  health {:.2f}  total {:.2f}  {:s} [{:05X}]",
				targetName ? targetName : "?", weaponName, weaponID,
				a_hit.physicalDamage, a_hit.resistedPhysicalDamage, a_hit.healthDamage, a_hit.totalDamage,
				Marks(a_hit), a_hit.flags.underlying());
		}

		// Logs a blow an NPC landed, in the NPC log. Only a weapon that wears
		// out, since those are the only ones whose damage this changes. Every
		// blow by every NPC comes here, so it checks for a file first.
		void LogNpcHit(const RE::HitData& a_hit, const RE::Actor& a_attacker)
		{
			if (!TraceLog::IsOpen()) {
				return;
			}

			auto* used = a_hit.weapon.object;
			if (!used || !used->IsWeapon()) {
				return;
			}
			const auto& weapon = static_cast<const RE::TESObjectWEAP&>(*used);
			if (!Condition::WearsOut(weapon)) {
				return;
			}

			const auto          target = a_hit.target.get();
			const TraceLog::Who who{ &a_attacker };
			const TraceLog::Who whom{ target.get() };
			const auto          name = RE::TESFullName::GetFullName(weapon);

			if (ByHand(a_hit)) {
				TraceLog::Npc::Begin(IsBash(a_hit, weapon) ? "BASH"sv : "MELEE"sv, "{:s} with {:s} [{:08X}] on {:s}",
					who, name, weapon.formID, whom);
			}

			TraceLog::Npc::Line("hit", "{:s} on {:s} with {:s} [{:08X}]  physical {:.2f}  resisted {:.2f}  health {:.2f}  total {:.2f}  {:s} [{:05X}]",
				who, whom, name, weapon.formID,
				a_hit.physicalDamage, a_hit.resistedPhysicalDamage, a_hit.healthDamage, a_hit.totalDamage,
				Marks(a_hit), a_hit.flags.underlying());
		}

		// Wears the player's weapon when a blow it struck lands, and logs
		// everybody else's blows.
		class HitSink : public RE::BSTEventSink<RE::TESHitEvent>
		{
		public:
			F4_HEAP_REDEFINE_NEW(HitSink);

		private:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent& a_event, RE::BSTEventSource<RE::TESHitEvent>*) override;
		};

		RE::BSEventNotifyControl HitSink::ProcessEvent(const RE::TESHitEvent& a_event, RE::BSTEventSource<RE::TESHitEvent>*)
		{
			if (!a_event.usesHitData) {
				return RE::BSEventNotifyControl::kContinue;
			}

			const auto& hit = a_event.hitData;
			const auto  aggressor = hit.aggressor.get();
			if (!aggressor) {
				return RE::BSEventNotifyControl::kContinue;
			}

			// Nobody's weapon wears but the player's, so anybody else's blow is
			// logged, and a blow with a blade like the Ripper is read for its
			// pace, see FireRate.h.
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (aggressor.get() != player) {
				LogNpcHit(hit, *aggressor);
				if (ByHand(hit)) {
					FireRate::NoteNpcWeapon(*aggressor, hit.weapon);
				}
				return RE::BSEventNotifyControl::kContinue;
			}

			const auto byHand = ByHand(hit);

			// The hit names the weapon that caused it. The game sends one hit
			// event per projectile, so a shotgun blast is one per pellet, which
			// is why guns take their wear per shot in FireHk. The kBash kind
			// arrives naming nothing, so the weapon in hand is the one that
			// struck. Vanilla never hands the player that attack, see IsBash.
			// It is kept to that flag alone, so a blow arriving empty for
			// another reason is passed over.
			auto* usedWeapon = hit.weapon.object;
			if (!usedWeapon && byHand && hit.flags.any(RE::HitData::Flag::kBash, RE::HitData::Flag::kTimedBash)) {
				usedWeapon = Equipped::Weapon(player);
			}

			// Worked out here because the log line names the kind of blow, and
			// IsBash reads the weapon's type.
			auto* weapon = usedWeapon && usedWeapon->IsWeapon() ?
			                   static_cast<RE::TESObjectWEAP*>(usedWeapon) :
			                   nullptr;
			const auto bash = byHand && weapon && IsBash(hit, *weapon);

			LogPlayerHit(hit, usedWeapon, byHand, bash);

			// Blows struck by hand only. A gun already wore for the shot in
			// FireHk. Bashing fires nothing, so it wears here, machete or
			// rifle.
			if (!byHand || !weapon) {
				return RE::BSEventNotifyControl::kContinue;
			}

			// A bash costs more than a swing, see BASH_WEAR, and a harder blow
			// costs more again, see AttackMult. The engine has released the
			// inventory lock by the time Wear returns, which makes the refresh
			// safe, see ItemCards.h.
			const auto scale = (bash ? BASH_WEAR : 1.0F) * AttackMult(hit);
			if (Condition::WearsOut(*weapon) &&
				WeaponWear::Wear(*player, *weapon, bash ? "bash" : "melee", scale)) {
				ItemCards::Refresh(RE::ENUM_FORM_ID::kWEAP);
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	}

	void RegisterHitSink()
	{
		// Once only. The game keeps the hit event source through every reload
		// and full reset, and a second sink would count every hit twice.
		static bool registered = false;
		if (registered) {
			return;
		}

		auto* source = RE::TESHitEvent::GetEventSource();
		if (!source) {
			REX::ERROR("No hit event source, so melee weapons will not wear down.");
			return;
		}
		source->RegisterSink(new HitSink());
		registered = true;
	}
}
