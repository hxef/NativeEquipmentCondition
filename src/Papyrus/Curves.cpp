#include "Papyrus/Natives.h"

#include "Condition/Condition.h"
#include "Core/TraceLog.h"
#include "Gameplay/Aim/Aim.h"
#include "Gameplay/ArmorRating.h"
#include "Gameplay/CritMeter.h"
#include "Gameplay/FireRate/FireRate.h"
#include "Gameplay/HealthDamage/HealthDamage.h"
#include "Gameplay/ItemValue.h"
#include "Gameplay/Jam.h"
#include "Papyrus/Items.h"

#include <string_view>
#include <variant>

// The curves, the number NEC applies at a condition, and what 1 use wears off
// an item. Each comes from the function NEC's hooks call, with the settings as
// they stand at the call.
namespace Papyrus
{
	namespace
	{
		float Result(float a_value)
		{
			TraceLog::Papyrus::Line("result", "{:.9g}", a_value);
			return a_value;
		}

		// The curve a_share at a_percent, for the curves that take nothing else.
		float Curve(std::string_view a_name, float a_percent, float (*a_share)(float))
		{
			Log::Calling(a_name);
			TraceLog::Papyrus::Begin("CALL", "NEC.{:s} percent={:.4f}", a_name, a_percent);
			return Result(a_share(HealthAt(a_percent)));
		}

		// a_form as a weapon record, or null for None and any other form.
		const RE::TESObjectWEAP* WeaponOf(RE::TESForm* a_form)
		{
			return a_form ? a_form->As<RE::TESObjectWEAP>() : nullptr;
		}
	}

	float GetDamageShare(std::monostate, float a_percent)
	{
		return Curve("GetDamageShare", a_percent, HealthDamage::DamageShare);
	}

	float GetArmorShare(std::monostate, float a_percent)
	{
		return Curve("GetArmorShare", a_percent, ArmorRating::Share);
	}

	float GetCritShare(std::monostate, float a_percent)
	{
		return Curve("GetCritShare", a_percent, CritMeter::Share);
	}

	float GetPriceShare(std::monostate, float a_percent)
	{
		return Curve("GetPriceShare", a_percent, ItemValue::Share);
	}

	float GetJamChance(std::monostate, float a_percent)
	{
		return Curve("GetJamChance", a_percent, Jam::ChancePerMagazine);
	}

	float GetAccuracyShare(std::monostate, RE::TESForm* a_weapon, float a_percent)
	{
		Log::Calling("GetAccuracyShare");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetAccuracyShare weapon={:s} percent={:.4f}", Hex(a_weapon), a_percent);
		const auto* weapon = WeaponOf(a_weapon);
		return Result(weapon ? Aim::AccuracyShare(*weapon, HealthAt(a_percent)) : 1.0F);
	}

	float GetRangeShare(std::monostate, RE::TESForm* a_weapon, float a_percent)
	{
		Log::Calling("GetRangeShare");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetRangeShare weapon={:s} percent={:.4f}", Hex(a_weapon), a_percent);
		const auto* weapon = WeaponOf(a_weapon);
		return Result(weapon ? Aim::RangeShare(*weapon, HealthAt(a_percent)) : 1.0F);
	}

	float GetFireRateShare(std::monostate, Item a_weapon, float a_percent)
	{
		Log::Calling("GetFireRateShare");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetFireRateShare item={:s} percent={:.4f}", Shown(a_weapon), a_percent);
		auto share = 1.0F;
		{
			// The copy's own stats, since a mod makes a weapon automatic.
			Holding     held{ a_weapon };
			const auto* found = held.found();
			const auto* weapon = found ? found->base->As<RE::TESObjectWEAP>() : nullptr;
			if (weapon) {
				const auto& data = Condition::InstanceDataOf<RE::TESObjectWEAP::InstanceData>(found->extra, weapon->weaponData);
				share = FireRate::RateShare(*weapon, &data, HealthAt(a_percent));
			}
		}
		return Result(share);
	}

	float GetWearPerUse(std::monostate, Item a_item)
	{
		Log::Calling("GetWearPerUse");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetWearPerUse item={:s}", Shown(a_item));
		auto points = -1.0F;
		{
			Holding held{ a_item };
			if (const auto* found = held.found(); found && Tracked(*found)) {
				points = PerUseOf(*found->base, found->extra, HeldByNpc(*found), found->crew) * 100.0F;
			}
		}
		return Result(points);
	}
}
