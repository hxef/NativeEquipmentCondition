#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <optional>

// Condition is the game's own health extra data on an inventory stack,
// MIN_HEALTH to MAX_HEALTH. Each kind of item has its own folder saying which
// items of that kind take part and how fast one wears, WeaponWear.h for weapons
// and ArmorWear.h for armor. Equipped.h reads what an actor holds and has on.
// What a worn weapon does to damage, price and jamming, and what a worn piece
// of armor does to protection, lives in Gameplay.
namespace Condition
{
	// Just under 1. SetHealthPerc at exactly 1.0 removes the extra data, and
	// above 1.0 the item is worth more, leftover Skyrim code.
	inline constexpr float MAX_HEALTH = 0.9999999F;

	// Broken is exactly 0, the value the game itself calls broken: IsItemBroken
	// returns true at 0 or less, and CanEquip then says "You must repair this
	// item before equipping it." It says so for an item being taken off too,
	// which BrokenEquip.h lets through. The unmodified game only writes a
	// health on power armor pieces and fusion cores, and only shows that
	// message at the power armor station, so for weapons and armor the rule is
	// there but never used. The engine's search for the equipped item also
	// returns 0 when it finds nothing, and EquippedHealthHk in
	// HealthDamage/Combat.cpp turns that into full condition.
	inline constexpr float MIN_HEALTH = 0.0F;

	// What a stack with no health extra data reads as. Such an item counts as
	// new.
	inline constexpr float INVALID_HEALTH = -1.0F;

	// The health a_stack carries, or INVALID_HEALTH for a stack carrying none
	// and for no stack at all, which count as new.
	float HealthOf(const RE::BGSInventoryItem::Stack* a_stack);

	// The instance data a_extra carries, or a_base, the record's own, when it
	// carries none. The engine only builds instance data for an item with a mod
	// list, and can leave the extra data entry with an empty pointer, so the
	// fallback checks the pointer and not the entry. a_extra can be null.
	template <class Data>
	[[nodiscard]] const Data& InstanceDataOf(const RE::ExtraDataList* a_extra, const Data& a_base)
	{
		const auto* extra = a_extra ? a_extra->GetByType<RE::ExtraInstanceData>() : nullptr;
		return extra && extra->data ? static_cast<const Data&>(*extra->data) : a_base;
	}

	// Whether this object takes part at all. Weapons and armor, and
	// WeaponWear.h and ArmorWear.h say which of each. Damage is not a test:
	// a laser has 0 physical damage and keeps it all in damage types.
	bool WearsOut(const RE::TESBoundObject& a_object);

	// The same question as a reason: nullptr for an object that takes part,
	// otherwise a short phrase for the trace log saying what ruled it out.
	// WearsOut is written in terms of this, so the 2 cannot disagree.
	const char* WhyNoCondition(const RE::TESBoundObject& a_object);

	// The 2 kinds of item that wear, for a sentence or a refresh that has to
	// name one. Only meaningful for an object WearsOut says yes to.
	enum class Kind
	{
		kWeapon,
		kArmor,
	};

	// Which kind a_object is: armor for an ARMO record, a weapon otherwise.
	Kind KindOf(const RE::TESBoundObject& a_object);

	// The condition a menu shows, as a whole percent, or nothing for an item
	// that does not wear. a_stack can be null. An item with no health extra
	// data counts as new, which the engine's reader reports as -1. Only an item
	// at full reads 100 and only a broken one reads 0.
	std::optional<double> Percent(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack);

	// a_health rounded to a whole percent, 0 to 100, where any health above 0
	// reads 1 at least. Percent and the HUD and Pip-Boy bars share it, so a
	// nearly broken item reads 1 on all of them.
	double WholePercent(float a_health);

	// The share of a full stat an item keeps at a_health: a straight line from
	// a_floor at 0 up to 1 at full. A health below 0, meaning none was ever
	// written, and one at MAX_HEALTH or above both count as new. At a floor of
	// 0.66 a gun at 0.5 keeps 0.83.
	float Share(float a_health, float a_floor);

	// The health to write for a whole percent. 100 becomes MAX_HEALTH, just
	// under 1, since the engine deletes a health set to exactly 1, and an item
	// with no health counts as never touched.
	float FromPercent(std::uint32_t a_percent);

	// Takes a_amount off the health in a_extra, kept between MIN_HEALTH and
	// MAX_HEALTH. a_object and a_source name the item and the event on the
	// trace line, gun fire or a melee hit, and a_scale is how many ordinary
	// uses the event counted as, already included in a_amount. a_theirs sends
	// the line to the NPC log, for an item someone else is wearing down.
	// Returns true when the health changed. Runs under the inventory lock, so
	// the caller acts on a change after releasing it.
	bool Decrease(RE::ExtraDataList& a_extra, const RE::TESBoundObject& a_object, float a_amount, const char* a_source, float a_scale,
		bool a_theirs = false);
}
