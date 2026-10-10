#include "Condition/WeaponWear/Rate.h"

#include "Condition/Condition.h"
#include "Condition/Materials/Materials.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <format>
#include <mutex>
#include <string>

namespace WeaponWear
{
	namespace
	{
		// Health the reference weapon loses per shot or swing, before
		// fWearRateMult. About 1100 uses from new to broken, long enough that
		// condition is something to keep an eye on. An fWearRateMult of 60 cuts
		// that to under 20, which is how the wear is tested. A fast automatic
		// gets more uses, see Rate.
		constexpr float RATE_AT_REFERENCE = 0.0009F;

		// What the reference weapon hits for. Measured by Load and
		// MeasureAgain, since an overhaul that multiplies every damage in the
		// game would otherwise wear everything out many times faster. Vanilla's
		// own value is used until then, and in a load order with no weapon
		// that hits. Atomic, since wear reads it on other threads.
		constexpr float    DEFAULT_REFERENCE_DAMAGE = 30.0F;
		std::atomic<float> g_referenceDamage{ DEFAULT_REFERENCE_DAMAGE };

		// The weapons whose damage sources the trace log has listed, by form
		// ID. Several threads wear weapons, hence the lock.
		std::mutex                        g_seenLock;
		std::unordered_set<std::uint32_t> g_seen;

		// How many times a second the ordinary automatic weapon fires, 9.09 in
		// vanilla. Measured with the damage, and 0 while the load order has
		// none, which leaves every weapon's wear as it is.
		std::atomic<float> g_referenceAutoRate{ 0.0F };

		// The middle value, the higher of the 2 in the middle for an even count.
		// 0 for none.
		float Median(std::vector<float>& a_values)
		{
			if (a_values.empty()) {
				return 0.0F;
			}
			const auto middle = a_values.begin() + a_values.size() / 2;
			std::nth_element(a_values.begin(), middle, a_values.end());
			return *middle;
		}

		// How many times a second an automatic weapon fires with these stats,
		// mods included. The Ripper and the buzz blade carry the game's
		// Automatic flag too, and the game counts 5 swings to each of their
		// attacks. 0 for any other weapon, and for a rate that is not a number
		// above 0. FireRate slows a worn gun only at the calls it patches, so
		// this call reads the full rate at any condition, and wear does not
		// climb as a gun wears.
		float AutoRate(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData& a_instance)
		{
			if (!a_instance.flags.any(RE::WEAPON_FLAGS::kAutomatic)) {
				return 0.0F;
			}
			const auto rate = RE::TESObjectWEAP::GetRateOfFire(a_weapon, &a_instance);
			return std::isfinite(rate) && rate > 0.0F ? rate : 0.0F;
		}

		// What the round does where it lands. A Fat Man's record reads 18
		// damage and a Broadsider's 33, since the damage belongs to the shell,
		// and counting the tube alone would have a Fat Man last 1500 mini
		// nukes. The engine adds the same number for the item card, see
		// CombatFormulas::GetWeaponDisplayDamage. Its own projectile lookup is
		// used, so a barrel that swaps the projectile and a change of
		// ammunition are both followed.
		float ExplosionDamage(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData& a_instance)
		{
			const auto* projectile = RE::TESObjectWEAP::GetProjectile(&a_weapon, a_instance.ammo, &a_instance);
			const auto  bursts = static_cast<std::uint32_t>(RE::BGSProjectile::BGSProjectileFlags::kExplosion);
			if (!projectile || !(projectile->data.flags & bursts)) {
				return 0;
			}

			// A projectile can be flagged to explode and name no explosion, a
			// record left half finished.
			const auto* explosion = projectile->data.explosionType;
			return explosion ? std::max(explosion->data.damage, 0.0F) : 0.0F;
		}

		// What the gun itself hits for, read the way the engine reads it. A
		// round's own damage is added to the weapon's, so ammunition that hits
		// harder wears the gun faster. Every vanilla round carries 0. A gun
		// with no ammunition reads secondaryDamage, the engine's own fallback.
		float BaseDamage(const RE::TESObjectWEAP::InstanceData& a_instance)
		{
			if (a_instance.ammo) {
				return static_cast<float>(a_instance.attackDamage) + a_instance.ammo->data.damage;
			}
			if (a_instance.type == RE::WEAPON_TYPE::kGun) {
				return a_instance.secondaryDamage;
			}
			return static_cast<float>(a_instance.attackDamage);
		}

		// Every kind of damage, not the physical alone. A laser keeps all of
		// its damage in damage types and reads 0 physical.
		float TotalDamage(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData& a_instance)
		{
			auto damage = BaseDamage(a_instance);
			if (a_instance.damageTypes) {
				for (const auto& entry : *a_instance.damageTypes) {
					// Whole numbers, which is how the engine reads them too.
					damage += static_cast<float>(entry.second.i);
				}
			}
			return damage + ExplosionDamage(a_weapon, a_instance);
		}

		// Prints the magic effects behind a spell a weapon casts on what it
		// hits, its object effect or its critical effect. The magnitude is what
		// a weapon in full condition applies to the target.
		void LogEffects(const char* a_what, const RE::MagicItem* a_item)
		{
			if (!a_item) {
				return;
			}

			TraceLog::Line("sources", "  {:s} {:s} [{:08X}], {:d} effects",
				a_what, RE::TESFullName::GetFullName(*a_item), a_item->formID, a_item->listOfEffects.size());

			for (const auto* effect : a_item->listOfEffects) {
				if (!effect) {
					continue;
				}
				const auto* setting = effect->effectSetting;
				TraceLog::Line("sources", "    {:s} [{:08X}] magnitude {:.2f}",
					setting ? RE::TESFullName::GetFullName(*setting) : "?"sv,
					setting ? setting->formID : 0, effect->data.magnitude);
			}
		}
	}

	void MeasureReference(bool a_again)
	{
		// The median weapon, not the average. A few hit for hundreds while most
		// hit for tens. The same for the automatics, where the Minigun fires 3
		// times as fast as most.
		std::vector<float> damages;
		std::vector<float> rates;
		for (auto* weapon : g_dataHandler->GetFormArray<RE::TESObjectWEAP>()) {
			if (!weapon || !Condition::WearsOut(*weapon)) {
				continue;
			}
			const auto damage = TotalDamage(*weapon, weapon->weaponData);
			if (damage > 0.0F) {
				damages.push_back(damage);
			}
			const auto rate = AutoRate(*weapon, weapon->weaponData);
			if (rate > 0.0F) {
				rates.push_back(rate);
			}
		}

		const auto ordinary = damages.empty() ? DEFAULT_REFERENCE_DAMAGE : Median(damages);
		const auto ordinaryRate = Median(rates);
		const auto damageBefore = g_referenceDamage.exchange(ordinary);
		const auto rateBefore = g_referenceAutoRate.exchange(ordinaryRate);

		// Again, a line only for a number that moved as the log prints it.
		if (a_again) {
			const auto said = std::format("{:.0f}", ordinary);
			const auto saidBefore = std::format("{:.0f}", damageBefore);
			if (said != saidBefore) {
				REX::INFO("An ordinary weapon in this load order now hits for {:s} instead of {:s}, as a mod changed weapons after game data loaded.",
					said, saidBefore);
			}
			const auto fires = std::format("{:.2f}", ordinaryRate);
			const auto firesBefore = std::format("{:.2f}", rateBefore);
			if (fires == firesBefore) {
				return;
			}
			if (ordinaryRate <= 0.0F) {
				REX::INFO("No automatic weapon in this load order wears now, as a mod changed weapons after game data loaded, so no weapon's wear is cut for firing fast.");
			} else if (rateBefore <= 0.0F) {
				REX::INFO("An ordinary automatic weapon in this load order now fires {:s} times a second, as a mod changed weapons after game data loaded.",
					fires);
			} else {
				REX::INFO("An ordinary automatic weapon in this load order now fires {:s} times a second instead of {:s}, as a mod changed weapons after game data loaded.",
					fires, firesBefore);
			}
			return;
		}

		// A full reset can give a form ID to another weapon, so each is listed
		// again.
		{
			const std::scoped_lock l(g_seenLock);
			g_seen.clear();
		}

		REX::INFO("An ordinary weapon in this load order hits for {:.0f}.", ordinary);
		if (ordinaryRate > 0.0F) {
			REX::INFO("An ordinary automatic weapon in this load order fires {:.2f} times a second.", ordinaryRate);
		} else {
			REX::INFO("No automatic weapon in this load order wears, so no weapon's wear is cut for firing fast.");
		}
	}

	float ReferenceDamage()
	{
		return g_referenceDamage.load();
	}

	void LogDamageSources(const RE::TESBoundObject& a_object, RE::TBO_InstanceData* a_data, bool a_perStack)
	{
		if (!a_object.IsWeapon() || !a_data) {
			return;
		}

		{
			const std::scoped_lock l(g_seenLock);
			if (!g_seen.insert(a_object.formID).second) {
				return;
			}
		}

		const auto* weapon = static_cast<const RE::TESObjectWEAP::InstanceData*>(a_data);

		TraceLog::Line("sources", "{:s} [{:08X}] from {:s} instance data, physical damage {:d}",
			RE::TESFullName::GetFullName(a_object), a_object.formID,
			a_perStack ? "per stack" : "base form", weapon->attackDamage);

		if (weapon->damageTypes && !weapon->damageTypes->empty()) {
			for (const auto& entry : *weapon->damageTypes) {
				const auto* type = entry.first;
				const auto* damage = type ? type->As<RE::BGSDamageType>() : nullptr;
				TraceLog::Line("sources", "  damage type {:s} [{:08X}] value {:d}",
					damage ? TraceLog::TypeName(*damage) : "?"sv,
					type ? type->formID : 0, entry.second.i);
			}
		} else {
			TraceLog::Line("sources", "  no damage types");
		}

		// The weapon's own object effect comes off the form, the ones its mods
		// add off this copy's data.
		LogEffects("object effect", static_cast<const RE::TESObjectWEAP&>(a_object).GetBaseEnchanting());
		if (weapon->enchantments) {
			for (const auto* effect : *weapon->enchantments) {
				LogEffects("mod object effect", effect);
			}
		}
		LogEffects("critical effect", weapon->effect);
	}

	float Rate(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData& a_instance,
		const RE::ExtraDataList* a_extra)
	{
		const auto damage = TotalDamage(a_weapon, a_instance);
		const auto blast = ExplosionDamage(a_weapon, a_instance);

		// What the weapon is built from, see Materials.h. This and the
		// references are read from the game as it is loaded.
		const auto quality = Materials::Quality(a_weapon, a_extra);

		// An automatic that fires faster than the ordinary automatic wears
		// less per use, as if it fired at the ordinary pace. A Minigun at 27.27
		// a second against 9.09 wears 1/3 as much per shot. Its bashes are
		// cut the same, since the Shredder's held bash lands about 19 blows a
		// second in first person. Every slower weapon keeps its full wear.
		const auto rate = AutoRate(a_weapon, a_instance);
		const auto ordinaryRate = g_referenceAutoRate.load();
		const auto pace = ordinaryRate > 0.0F && rate > ordinaryRate ? ordinaryRate / rate : 1.0F;

		// Harder hitting wears faster, better built wears slower. A pipe gun is
		// steel and hits for little, a plasma rifle is nuclear material and
		// hits hard, and both land near the middle. A pipe rifle rechambered to
		// .50 wears out fast, and a well made gun with low damage lasts. Both
		// are measured against the reference weapon, so how much damage a
		// weapon can deal in its life depends on what it is built from, and
		// damage per shot decides how fast that runs out.
		// fWearRateMult stops at 0, since below it a gun would gain condition
		// as it fires.
		const auto mult = Settings::fWearRateMult.GetValue();
		const auto wear = RATE_AT_REFERENCE * (mult > 0.0F ? mult : 0.0F) *
		                  (damage / g_referenceDamage.load()) * (Materials::ReferenceQuality(Condition::Kind::kWeapon) / quality) * pace;

		const auto cut = TraceLog::IsOpen() && pace < 1.0F ? std::format(", cut x{:.4f} for firing {:.2f} a second", pace, rate) : std::string{};
		if (blast > 0.0F) {
			TraceLog::Line("rate", "{:.0f} damage, {:.0f} of it the blast, at quality {:.1f} costs {:.6f} a shot{:s}",
				damage, blast, quality, wear, cut);
		} else {
			TraceLog::Line("rate", "{:.0f} damage at quality {:.1f} costs {:.6f} a shot{:s}", damage, quality, wear, cut);
		}
		return wear;
	}
}
