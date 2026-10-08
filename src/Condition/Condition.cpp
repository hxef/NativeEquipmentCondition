#include "Condition/Condition.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/WeaponWear/WeaponWear.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

namespace Condition
{
	const char* WhyNoCondition(const RE::TESBoundObject& a_object)
	{
		// One case per kind of item that wears. The form type byte costs
		// nothing to read and never comes back empty like a runtime cast can.
		if (a_object.IsWeapon()) {
			return WeaponWear::WhyNoCondition(static_cast<const RE::TESObjectWEAP&>(a_object));
		}
		if (a_object.Is(RE::ENUM_FORM_ID::kARMO)) {
			return ArmorWear::WhyNoCondition(static_cast<const RE::TESObjectARMO&>(a_object));
		}
		return "not a weapon or armor";
	}

	bool WearsOut(const RE::TESBoundObject& a_object)
	{
		return WhyNoCondition(a_object) == nullptr;
	}

	Kind KindOf(const RE::TESBoundObject& a_object)
	{
		return a_object.Is(RE::ENUM_FORM_ID::kARMO) ? Kind::kArmor : Kind::kWeapon;
	}

	std::optional<double> Percent(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack)
	{
		if (!a_item.object || !WearsOut(*a_item.object)) {
			return std::nullopt;
		}

		// Below 0, not at it. -1 is a stack with no health extra data, which
		// counts as new. 0 is broken and shows as 0%.
		auto health = HealthOf(a_stack);
		if (health < 0.0F) {
			health = 1.0F;
		}

		// The bench and the traders decide from this number whether an item is
		// worn. Rounding alone would show a gun fired a few times as 100, with
		// no repair on offer and its mod slots open, so any wear reads 99 at
		// most.
		const auto percent = std::round(std::clamp(health, 0.0F, 1.0F) * 100.0);
		return health < MAX_HEALTH ? std::min(percent, 99.0) : percent;
	}

	float Share(float a_health, float a_floor)
	{
		// -1 is an item with no health extra data, which counts as new. 0 and
		// up is a real health, 0 being broken. EquippedHealthHk in
		// HealthDamage/Combat.cpp keeps that true on the combat path.
		if (a_health < 0.0F || a_health >= MAX_HEALTH) {
			return 1.0F;
		}
		return a_floor + ((1.0F - a_floor) * a_health);
	}

	float FromPercent(std::uint32_t a_percent)
	{
		return a_percent >= 100 ? MAX_HEALTH : static_cast<float>(a_percent) / 100.0F;
	}

	float HealthOf(const RE::BGSInventoryItem::Stack* a_stack)
	{
		return a_stack && a_stack->extra ? a_stack->extra->GetHealthPerc() : INVALID_HEALTH;
	}

	namespace
	{
		// Writes the health, clamped, and returns what was written.
		float SetHealth(RE::ExtraDataList& a_extra, float a_health)
		{
			if (a_health < MIN_HEALTH) {
				a_health = MIN_HEALTH;
			} else if (a_health >= MAX_HEALTH) {
				a_health = MAX_HEALTH;
			}
			a_extra.SetHealthPerc(a_health);
			return a_health;
		}
	}

	bool Decrease(RE::ExtraDataList& a_extra, const RE::TESBoundObject& a_object, float a_amount, const char* a_source, float a_scale,
		bool a_theirs)
	{
		// -1 is an item this has never touched. Subtracting from that would
		// clamp the first hit to 0 and the item would arrive broken.
		auto curHealth = a_extra.GetHealthPerc();
		if (curHealth < 0.0F) {
			curHealth = MAX_HEALTH;
		}

		// The scale is logged whenever there is one, so the log shows what a
		// bash, a power attack or a charged shot cost.
		const auto newHealth = SetHealth(a_extra, curHealth - a_amount);
		const auto scale = a_scale != 1.0F ? std::format(" at x{:.2f}", a_scale) : std::string{};
		TraceLog::For(a_theirs).Line("wear", "{:s} [{:08X}] {:.6f} -> {:.6f} (-{:.6f}) from {:s}{:s}",
			RE::TESFullName::GetFullName(a_object), a_object.formID, curHealth, newHealth, a_amount, a_source, scale);

		// Only a change of health counts, so an item already at 0 sets nothing
		// off again on every shot or hit.
		return newHealth != curHealth;
	}
}
