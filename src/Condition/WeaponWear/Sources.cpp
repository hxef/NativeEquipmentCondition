#include "Condition/WeaponWear/Sources.h"

#include "Core/TraceLog/TraceLog.h"

#include <cstdint>
#include <mutex>
#include <unordered_set>

namespace WeaponWear
{
	namespace
	{
		// The weapons whose damage sources the trace log has listed, by form
		// ID. Several threads wear weapons, hence the lock.
		std::mutex                        g_seenLock;
		std::unordered_set<std::uint32_t> g_seen;

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

	void ForgetSources()
	{
		const std::scoped_lock l(g_seenLock);
		g_seen.clear();
	}
}
