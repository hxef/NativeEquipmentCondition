#include "Condition/Condition.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/WeaponWear/WeaponWear.h"
#include "Core/Settings.h"
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
		const auto percent = WholePercent(health);
		return health < MAX_HEALTH ? std::min(percent, 99.0) : percent;
	}

	double WholePercent(float a_health)
	{
		// An item under half a percent still works, so any health above 0
		// reads 1 at least and only a broken item reads 0.
		const auto percent = std::round(std::clamp(a_health, 0.0F, 1.0F) * 100.0);
		return a_health > MIN_HEALTH ? std::max(percent, 1.0) : percent;
	}

	float Start(Kind a_kind)
	{
		const auto start = (a_kind == Kind::kArmor ? Settings::fArmorPenaltyStart : Settings::fWeaponPenaltyStart).GetValue();
		return start < 1.0F ? std::max(start, 0.0F) : 1.0F;
	}

	float Share(float a_health, float a_floor, Kind a_kind)
	{
		// -1 is an item with no health extra data, which counts as new. 0 and
		// up is a real health, 0 being broken. EquippedHealthHk in
		// HealthDamage/Combat.cpp keeps that true on the combat path.
		if (a_health < 0.0F || a_health >= MAX_HEALTH) {
			return 1.0F;
		}

		// A broken item is at its floor whatever the start, so at a start of
		// 0 an item keeps all of it until it breaks. The line only runs for a
		// start above 0, so it never divides by 0.
		const auto start = Start(a_kind);
		if (a_health <= MIN_HEALTH) {
			return a_floor;
		}
		if (a_health >= start) {
			return 1.0F;
		}
		return a_floor + ((1.0F - a_floor) * (a_health / start));
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
		// Writes the health, clamped, and returns what was written. The clamp
		// stops at MAX_HEALTH, so this never writes UNROLLED.
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

		// The health a wear starts from. -1 is an item this has never touched.
		// Subtracting from that would clamp the first hit to 0 and the item
		// would arrive broken.
		float Before(float a_health)
		{
			return a_health < 0.0F ? MAX_HEALTH : a_health;
		}

		// Whether taking a_amount off a_before changes it. Nothing comes off at
		// a wear speed of 0 or for an amount too small to change the number. A
		// write would still give an item never touched a health of its own,
		// which the engine keeps apart from its stack. The same test turns away
		// a NaN or a negative amount. An UNROLLED item used at a wear speed of
		// 0 stays UNROLLED and still reads as full in Percent and Share. An item
		// at 0 stays at 0, since the write stops there.
		bool Changes(float a_before, float a_amount)
		{
			return a_before > MIN_HEALTH && a_before - a_amount < a_before;
		}
	}

	bool Lowers(float a_health, const RE::TESBoundObject& a_object, float a_amount, const char* a_source, bool a_theirs)
	{
		const auto before = Before(a_health);
		if (Changes(before, a_amount)) {
			return true;
		}
		TraceLog::For(a_theirs).Line("wear", "{:s} [{:08X}] at {:.6f} loses nothing from {:s}, so it is not written",
			RE::TESFullName::GetFullName(a_object), a_object.formID, before, a_source);
		return false;
	}

	bool Decrease(RE::ExtraDataList& a_extra, const RE::TESBoundObject& a_object, float a_amount, const char* a_source, float a_scale,
		bool a_theirs)
	{
		const auto curHealth = Before(a_extra.GetHealthPerc());
		if (!Changes(curHealth, a_amount)) {
			return false;
		}

		// The scale is logged whenever there is one, so the log shows what a
		// bash, a power attack or a charged shot cost.
		const auto newHealth = SetHealth(a_extra, curHealth - a_amount);
		const auto scale = a_scale != 1.0F ? std::format(" at x{:.2f}", a_scale) : std::string{};
		TraceLog::For(a_theirs).Line("wear", "{:s} [{:08X}] {:.6f} -> {:.6f} (-{:.6f}) from {:s}{:s}",
			RE::TESFullName::GetFullName(a_object), a_object.formID, curHealth, newHealth, a_amount, a_source, scale);
		return newHealth != curHealth;
	}
}
