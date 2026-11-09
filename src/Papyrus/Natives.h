#pragma once

#include "Core/Plugin.h"

#include "Papyrus/Items.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// The natives of NEC.psc.in as the VM calls them, each with std::monostate
// first in place of the object a global function has none of. Register.cpp
// binds them. A text arrives as a std::string of its own, since a
// std::string_view would point into a string the VM lets go of first.
// Private to this folder.
namespace Papyrus
{
	// The version of NEC.dll as 1 number, major times 10000 plus minor times
	// 100 plus patch, so 10900 for 1.9.0, and as text, "1.9.0".
	[[nodiscard]] std::int32_t GetVersion(std::monostate);
	[[nodiscard]] std::string  GetVersionString(std::monostate);

	// A row of Features.cpp with a part, by its name: whether it runs, and
	// whether another DLL took any of it.
	[[nodiscard]] bool IsFeatureOn(std::monostate, std::string a_feature);
	[[nodiscard]] bool IsFeatureTakenOver(std::monostate, std::string a_feature);

	// What IsFeatureOn answers, for a native that asks it under its own name.
	// No native itself, so Register.cpp does not bind it.
	[[nodiscard]] bool FeatureRuns(std::string_view a_feature);

	// A setting of NEC.ini by its MCM id, such as fWearRateMult:Balance, as it
	// stands now. 0, 0.0 or false for an id NEC does not know.
	[[nodiscard]] float        GetSettingFloat(std::monostate, std::string a_id);
	[[nodiscard]] std::int32_t GetSettingInt(std::monostate, std::string a_id);
	[[nodiscard]] bool         GetSettingBool(std::monostate, std::string a_id);

	// 1 handle per stack, see Handles.cpp. None for a None holder or actor
	// and while the inventory is busy.
	[[nodiscard]] std::optional<std::vector<Item>> GetItems(std::monostate, RE::TESObjectREFR* a_holder, RE::TESForm* a_base);
	[[nodiscard]] Item                             GetEquippedWeapon(std::monostate, RE::Actor* a_actor);
	[[nodiscard]] Item                             GetWornItem(std::monostate, RE::Actor* a_actor, std::int32_t a_slot);
	[[nodiscard]] std::optional<std::vector<Item>> GetWornItems(std::monostate, RE::Actor* a_actor);

	// What an item names, see Items.cpp. None or 0 for None or a gone item.
	[[nodiscard]] RE::TESObjectREFR* GetHolder(std::monostate, Item a_item);
	[[nodiscard]] RE::TESForm*       GetBase(std::monostate, Item a_item);
	[[nodiscard]] std::int32_t       GetCount(std::monostate, Item a_item);

	// The writes, see Writer.cpp. True once the item is at the condition
	// asked, false for None, a gone item and gear NEC gives no condition.
	bool SetCondition(std::monostate, Item a_item, float a_percent);
	bool Damage(std::monostate, Item a_item, float a_percent);
	bool Repair(std::monostate, Item a_item, float a_percent);
	bool Wear(std::monostate, Item a_item, float a_uses);
	bool RollCondition(std::monostate, Item a_item);

	// The readers, see Readers.cpp. False, -1 or a reason for None, a gone
	// item and gear NEC gives no condition.
	[[nodiscard]] bool        IsTracked(std::monostate, RE::TESForm* a_base);
	[[nodiscard]] std::string WhyNotTracked(std::monostate, RE::TESForm* a_base);
	[[nodiscard]] float       GetCondition(std::monostate, Item a_item);
	[[nodiscard]] bool        IsBroken(std::monostate, Item a_item);
	[[nodiscard]] bool        IsUnrolled(std::monostate, Item a_item);

	// The curves, see Curves.cpp: what NEC applies at a percent of
	// condition, from the settings as they stand.
	[[nodiscard]] float GetDamageShare(std::monostate, float a_percent);
	[[nodiscard]] float GetArmorShare(std::monostate, float a_percent);
	[[nodiscard]] float GetAccuracyShare(std::monostate, RE::TESForm* a_weapon, float a_percent);
	[[nodiscard]] float GetRangeShare(std::monostate, RE::TESForm* a_weapon, float a_percent);
	[[nodiscard]] float GetFireRateShare(std::monostate, Item a_weapon, float a_percent);
	[[nodiscard]] float GetCritShare(std::monostate, float a_percent);
	[[nodiscard]] float GetPriceShare(std::monostate, float a_percent);
	[[nodiscard]] float GetJamChance(std::monostate, float a_percent);
	[[nodiscard]] float GetWearPerUse(std::monostate, Item a_item);

	// Struct Cost of NEC.psc.in, 1 component of a bill and its count.
	using Cost = RE::BSScript::structure_wrapper<"NEC", "Cost">;

	// The repair economics, see Repair.cpp, each from the code the bench or
	// the barter screen prices with.
	[[nodiscard]] std::int32_t                     GetFreeMendAbove(std::monostate);
	[[nodiscard]] std::int32_t                     GetModLockBelow(std::monostate);
	[[nodiscard]] std::int32_t                     GetRepairDiscount(std::monostate, RE::TESForm* a_base);
	[[nodiscard]] std::optional<std::vector<Cost>> GetRepairCost(std::monostate, Item a_item, std::int32_t a_toPercent);
	[[nodiscard]] std::optional<std::vector<Cost>> GetScrapYield(std::monostate, Item a_item);
	[[nodiscard]] std::int32_t                     GetSpareGain(std::monostate, Item a_item, Item a_spare);
	[[nodiscard]] std::int32_t                     GetTraderRepairLevel(std::monostate, RE::Actor* a_trader, RE::TESForm* a_base);
	[[nodiscard]] std::int32_t                     GetTraderRepairPrice(std::monostate, RE::Actor* a_trader, Item a_item, std::int32_t a_toPercent);

	// Food, see Food.cpp. -1 or false for None, a gone item and food NEC
	// does not spoil.
	[[nodiscard]] std::int32_t GetFoodKind(std::monostate, RE::TESForm* a_food);
	[[nodiscard]] float        GetSpoilDays(std::monostate, RE::TESForm* a_food);
	[[nodiscard]] float        GetFreshness(std::monostate, Item a_item);
	[[nodiscard]] float        GetDaysLeft(std::monostate, Item a_item);
	bool                       SetFreshness(std::monostate, Item a_item, float a_percent);
	bool                       Spoil(std::monostate, Item a_item);
	[[nodiscard]] bool         IsSpoiled(std::monostate, Item a_item);
	[[nodiscard]] bool         IsFridge(std::monostate, RE::TESObjectREFR* a_container);
}
