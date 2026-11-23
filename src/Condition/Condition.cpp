#include "Condition/Condition.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/Excluded.h"
#include "Condition/WeaponWear/WeaponWear.h"
#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <utility>

namespace Condition
{
	namespace
	{
		// a_health, kept between 0 and 1, as a whole count of a_steps to full,
		// rounded half up. An item under half a step still works, so any
		// health above 0 reads 1 at least and only a broken item reads 0.
		// WholePercent counts 100 steps and CardTenths 1000.
		double Rounded(float a_health, double a_steps)
		{
			const auto steps = std::round(std::clamp(a_health, 0.0F, 1.0F) * a_steps);
			return a_health > MIN_HEALTH ? std::max(steps, 1.0) : steps;
		}

		// Whether a card writes a_tenths with its tenth, at most FULL_TENTHS.
		// A full item never does, so it reads 100 and not 100.0, which would
		// not fit beside a 4 letter CND word and the wear rate.
		bool WithTenth(std::uint32_t a_tenths)
		{
			const auto decimals = Decimals();
			return a_tenths < FULL_TENTHS && decimals != CardDecimals::kOff &&
			       (decimals == CardDecimals::kAlways || a_tenths % TENTHS_PER_PERCENT != 0);
		}

		// a_tenths as text, 980 as "98.0" with a_tenth and "98" without. Whole
		// numbers only, so no language's decimal mark can creep in.
		std::string Written(std::uint32_t a_tenths, bool a_tenth)
		{
			const auto whole = a_tenths / TENTHS_PER_PERCENT;
			return a_tenth ? std::format("{:d}.{:d}", whole, a_tenths % TENTHS_PER_PERCENT) : std::format("{:d}", whole);
		}
	}

	const char* KindWhyNoCondition(const RE::TESBoundObject& a_object)
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

	bool KindWearsOut(const RE::TESBoundObject& a_object)
	{
		return KindWhyNoCondition(a_object) == nullptr;
	}

	const char* WhyNoCondition(const RE::TESBoundObject& a_object)
	{
		// The kind first, so a listed power armor piece still reads as power
		// armor.
		if (const auto* why = KindWhyNoCondition(a_object)) {
			return why;
		}
		if (Excluded::Has(a_object)) {
			return "on a list in NEC/Exclude";
		}
		// Past the kind's rule, a piece that gives nothing is one ArmorWear
		// lets in for being playable. The switch is tested here and not in
		// that rule, so the measures over the load order stay the same
		// whatever it says.
		if (Settings::bPlainApparelNeverWears.GetValue() && a_object.Is(RE::ENUM_FORM_ID::kARMO) &&
			!ArmorWear::GivesAnything(static_cast<const RE::TESObjectARMO&>(a_object))) {
			return "gives nothing, and plain apparel never wears";
		}
		return nullptr;
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
		return a_item.object ? Percent(*a_item.object, a_stack ? a_stack->extra.get() : nullptr) : std::nullopt;
	}

	std::optional<double> Percent(const RE::TESBoundObject& a_object, RE::ExtraDataList* a_extra)
	{
		if (!WearsOut(a_object)) {
			return std::nullopt;
		}

		return ShownPercent(HealthOf(a_extra));
	}

	double WholePercent(float a_health)
	{
		return Rounded(a_health, 100.0);
	}

	double ShownPercent(float a_health)
	{
		// Below 0, not at it. INVALID_HEALTH, -1, is a stack with no health
		// extra data, which counts as new. 0 is broken and shows as 0%.
		if (a_health < 0.0F) {
			return 100.0;
		}

		// The bench and the traders decide from this number whether an item is
		// worn. Rounding alone would show a gun fired a few times as 100, with
		// no repair on offer and its mod slots open, so any wear reads 99 at
		// most.
		const auto percent = WholePercent(a_health);
		return a_health < MAX_HEALTH ? std::min(percent, 99.0) : percent;
	}

	CardDecimals Decimals()
	{
		switch (Settings::iCndDecimals.GetValue()) {
		case std::to_underlying(CardDecimals::kOff):
			return CardDecimals::kOff;
		case std::to_underlying(CardDecimals::kFractions):
			return CardDecimals::kFractions;
		default:
			return CardDecimals::kAlways;
		}
	}

	std::uint32_t CardTenths(float a_health, bool a_wears)
	{
		if (Decimals() == CardDecimals::kOff) {
			return static_cast<std::uint32_t>(a_wears ? ShownPercent(a_health) : WholePercent(a_health)) * TENTHS_PER_PERCENT;
		}

		// As ShownPercent: no health extra data counts as new, and any wear
		// on gear reads 99.9 at most, so only an item at full reads 100.
		if (a_health < 0.0F) {
			return FULL_TENTHS;
		}
		const auto tenths = Rounded(a_health, static_cast<double>(FULL_TENTHS));
		return static_cast<std::uint32_t>(a_wears && a_health < MAX_HEALTH ? std::min(tenths, static_cast<double>(MOST_WORN_TENTHS)) : tenths);
	}

	std::optional<std::uint32_t> Tenths(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack)
	{
		return a_item.object ? Tenths(*a_item.object, a_stack ? a_stack->extra.get() : nullptr) : std::nullopt;
	}

	std::optional<std::uint32_t> Tenths(const RE::TESBoundObject& a_object, RE::ExtraDataList* a_extra)
	{
		if (!WearsOut(a_object)) {
			return std::nullopt;
		}

		return CardTenths(HealthOf(a_extra), true);
	}

	std::string CardText(std::uint32_t a_tenths)
	{
		const auto tenths = std::min(a_tenths, FULL_TENTHS);
		return Written(tenths, WithTenth(tenths));
	}

	std::string PointText(std::uint32_t a_tenths)
	{
		return Written(a_tenths, a_tenths % TENTHS_PER_PERCENT != 0);
	}

	std::string TenthText(std::uint32_t a_tenths)
	{
		return Written(a_tenths, true);
	}

	double CardValue(std::uint32_t a_tenths)
	{
		// A cut after 1 decimal or after 2, as a UI mod's list column cuts,
		// still gives the tenth: 97.505 prints as 97.5 and 97.50.
		const auto tenths = std::min(a_tenths, FULL_TENTHS);
		const auto value = static_cast<double>(tenths) / static_cast<double>(TENTHS_PER_PERCENT);
		return WithTenth(tenths) ? value + 0.005 : value;
	}

	float Start(Kind a_kind)
	{
		const auto start = (a_kind == Kind::kArmor ? Settings::fArmorPenaltyStart : Settings::fWeaponPenaltyStart).GetValue();
		return start < 1.0F ? std::max(start, 0.0F) : 1.0F;
	}

	std::int32_t StartPercent(Kind a_kind)
	{
		const auto percent = static_cast<std::int32_t>(std::lround(static_cast<double>(Start(a_kind)) * 100.0));
		return percent >= 1 && percent <= 99 ? percent : 0;
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

	float FromTenths(std::uint32_t a_tenths)
	{
		// Both divide 2 whole numbers to the same quotient, so 700 / 1000
		// is the same float as 70 / 100.
		return a_tenths >= FULL_TENTHS ? MAX_HEALTH : static_cast<float>(a_tenths) / static_cast<float>(FULL_TENTHS);
	}

	float HealthOf(RE::ExtraDataList* a_extra)
	{
		return a_extra ? a_extra->GetHealthPerc() : INVALID_HEALTH;
	}

	float HealthOf(const RE::BGSInventoryItem::Stack* a_stack)
	{
		return a_stack ? HealthOf(a_stack->extra.get()) : INVALID_HEALTH;
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
		if (!Settings::NecOn()) {
			return false;
		}
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
