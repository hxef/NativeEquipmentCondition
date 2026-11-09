#include "Core/Settings.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Runtime.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <format>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// The Settings line NEC.log writes, every setting as it stands, and the lists
// of switches and numbers it walks in NEC.ini's order, which the MCM page and
// the Papyrus API read too.
namespace Settings
{
	namespace
	{
		// -------------------------------------------------------------------
		// The lists
		// -------------------------------------------------------------------

		Live<bool>* const SWITCHES[]{
			&bSpawnCondition,
			&bVendorRepair,
			&bLoadingTips,
			&bHudCondition,
			&bQuickContainer,
			&bConfirmScroll,
			&bInspectPrice,
			&bFoodSpoils,
		};

		const Number NUMBERS[]{
			&iUnscrappableSpares,
			&iRepairBoxRow,
			&iLootShift,
			&fWearRateMult,
			&fNpcWearRateMult,
			&fAutoWearRateMult,
			&fArmorWearRateMult,
			&fNpcArmorWearRateMult,
			&fDamageFloor,
			&fWeaponPenaltyStart,
			&fArmorFloor,
			&fArmorPenaltyStart,
			&fValueExponent,
			&fFireRateFloor,
			&fCritMeterFloor,
			&fAccuracyFloor,
			&fRangeFloor,
			&fJamChance,
			&fJamStart,
			&fBenchCostMult,
			&fSpareWorthMult,
			&iFreeMendAbove,
			&fModLockStart,
			&fTraderPriceMult,
			&fRawFoodDays,
			&fCookedFoodDays,
			&fFridgeSpoilSpeed,
			&iSpoiledPerFertilizer,
			&iSpoiledHealing,
			&iSpoiledHunger,
			&fSpoiledRadsMult,
			&iSpoiledSicknessMult,
			&fHudBarX,
			&fHudBarY,
			&fHudBarLength,
			&fHudBarThickness,
			&fPowerArmorBarX,
			&fPowerArmorBarY,
			&fPowerArmorBarLength,
			&fPowerArmorBarThickness,
			&fPickupBarX,
			&fPickupBarY,
			&fPickupBarLength,
			&fPickupBarThickness,
		};

		// Every switch in NEC.ini's order, for a lookup by id.
		Live<bool>* const EVERY_SWITCH[]{
			&bPlainApparelNeverWears,
			&bSpawnCondition,
			&bVendorRepair,
			&bSpareRepair,
			&bRepairBoxColumns,
			&bLoadingTips,
			&bHudCondition,
			&bPowerArmorBar,
			&bQuickContainer,
			&bConfirmScroll,
			&bInspectPrice,
			&bFoodSpoils,
			&bFridgeIcon,
			&bHudBarHidesWithCounter,
			&bHudBarStandsAlone,
			&bHudBarShowsWord,
			&bPowerArmorBarShowsWord,
			&bPickupBar,
			&bPickupBarShowsWord,
			&bTraceLogs,
		};

		// The switches SWITCHES leaves out, each after the setting NEC.ini has
		// it after, for the Settings line. bPlainApparelNeverWears comes first
		// in [Features], so ReportLine says it before the list.
		Live<bool>* const AFTER_VENDOR_REPAIR[]{ &bSpareRepair, &bRepairBoxColumns };
		Live<bool>* const AFTER_HUD_CONDITION[]{ &bPowerArmorBar };
		Live<bool>* const AFTER_FOOD_SPOILS[]{ &bFridgeIcon };
		Live<bool>* const AFTER_HUD_BAR[]{ &bHudBarHidesWithCounter, &bHudBarStandsAlone, &bHudBarShowsWord };
		Live<bool>* const AFTER_POWER_ARMOR_BAR[]{ &bPowerArmorBarShowsWord, &bPickupBar };
		Live<bool>* const AFTER_PICKUP_BAR[]{ &bPickupBarShowsWord };

		// The numbers under [Features], which the Settings line puts after the
		// switch NEC.ini has them after.
		Live<std::int32_t>* const AFTER_SPARE_REPAIR[]{ &iUnscrappableSpares, &iRepairBoxRow };

		[[nodiscard]] std::span<Live<std::int32_t>* const> NumbersAfter(const Named& a_setting)
		{
			if (&a_setting == &bSpareRepair) {
				return AFTER_SPARE_REPAIR;
			}
			return {};
		}

		[[nodiscard]] std::span<Live<bool>* const> SwitchesAfter(const Named& a_setting)
		{
			if (&a_setting == &bVendorRepair) {
				return AFTER_VENDOR_REPAIR;
			}
			if (&a_setting == &bHudCondition) {
				return AFTER_HUD_CONDITION;
			}
			if (&a_setting == &bFoodSpoils) {
				return AFTER_FOOD_SPOILS;
			}
			if (&a_setting == &fHudBarThickness) {
				return AFTER_HUD_BAR;
			}
			if (&a_setting == &fPowerArmorBarThickness) {
				return AFTER_POWER_ARMOR_BAR;
			}
			if (&a_setting == &fPickupBarThickness) {
				return AFTER_PICKUP_BAR;
			}
			return {};
		}

		// -------------------------------------------------------------------
		// How the line shows a setting
		// -------------------------------------------------------------------

		// Who has a switch's part, short for the Settings line: the DLLs,
		// another mod, or the game version, one NEC does not support or one
		// with no place for it.
		std::string Owned(std::span<const CallPatch::Owner> a_owners)
		{
			std::string text;
			for (const auto& name : CallPatch::OwnerNames(a_owners, "another mod")) {
				text += text.empty() ? name : ", " + name;
			}
			if (!text.empty()) {
				return text;
			}
			return Runtime::Supported() ? std::format("no place on game version {:s}", Runtime::GameVersion()) :
			                              std::format("game version {:s} not supported", Runtime::GameVersion());
		}

		// A setting's mark on the Settings line while pieces it works through
		// are off: (no effect), or (some) while it still does part of its job.
		// An idle setting gets none, see CallPatch::Effect. The summary, see
		// CallPatch::Summary, says which pieces.
		std::string_view Mark(const Named& a_setting)
		{
			const auto effect = CallPatch::EffectOf(a_setting);
			return effect.idle ? ""sv : effect.None() ? "(no effect)"sv : !effect.off.empty() ? "(some)"sv : ""sv;
		}

		// A switch as the Settings line prints it. A switch left to another
		// mod says to whom, and one that is on carries its mark.
		std::string Shown(const Live<bool>& a_switch)
		{
			if (const auto yield = CallPatch::YieldOf(a_switch)) {
				std::vector<CallPatch::Owner> owners;
				for (const auto& cause : yield->causes) {
					for (const auto& owner : cause.owners) {
						if (std::ranges::find(owners, owner) == owners.end()) {
							owners.push_back(owner);
						}
					}
				}
				std::ranges::sort(owners, CallPatch::Before);
				return std::format("false({:s})", Owned(owners));
			}
			return a_switch.GetValue() ? std::format("true{:s}", Mark(a_switch)) : "false";
		}

		// A number as the Settings line prints it.
		std::string Shown(float a_value)
		{
			return std::format("{:g}", a_value);
		}

		std::string Shown(std::int32_t a_value)
		{
			return std::format("{:d}", a_value);
		}
	}

	std::span<Live<bool>* const> Switches()
	{
		return SWITCHES;
	}

	std::span<const Number> Numbers()
	{
		return NUMBERS;
	}

	bool IsIdOf(const Named& a_setting, std::string_view a_id)
	{
		const auto same = [](std::string_view a_lhs, std::string_view a_rhs) {
			return std::ranges::equal(a_lhs, a_rhs, [](char a_l, char a_r) {
				return std::tolower(static_cast<unsigned char>(a_l)) == std::tolower(static_cast<unsigned char>(a_r));
			});
		};
		const auto key = a_setting.key.size();
		return a_id.size() == key + 1 + a_setting.section.size() && a_id[key] == ':' &&
		       same(a_id.substr(0, key), a_setting.key) && same(a_id.substr(key + 1), a_setting.section);
	}

	Live<bool>* SwitchById(std::string_view a_id)
	{
		const auto it = std::ranges::find_if(EVERY_SWITCH, [&](const Live<bool>* a_switch) { return IsIdOf(*a_switch, a_id); });
		return it != std::end(EVERY_SWITCH) ? *it : nullptr;
	}

	// -------------------------------------------------------------------
	// The Settings line
	// -------------------------------------------------------------------

	void ReportLine(bool a_always)
	{
		// Every key as it stands now, before any limit, in NEC.ini's order and
		// spelling.
		std::string line = std::format("Settings: [Features] {:s}={:s}", bPlainApparelNeverWears.key, Shown(bPlainApparelNeverWears));
		const auto say = [&](const auto& a_number) {
			std::format_to(std::back_inserter(line), " {:s}={:s}{:s}", a_number.key, Shown(a_number.GetValue()), Mark(a_number));
		};
		for (const auto* on : SWITCHES) {
			std::format_to(std::back_inserter(line), " {:s}={:s}", on->key, Shown(*on));
			for (const auto* after : SwitchesAfter(*on)) {
				std::format_to(std::back_inserter(line), " {:s}={:s}", after->key, Shown(*after));
				for (const auto* number : NumbersAfter(*after)) {
					say(*number);
				}
			}
		}
		std::string_view section;
		for (const auto& entry : NUMBERS) {
			std::visit([&](const auto* a_number) {
				// Said after its switch above.
				if (a_number->section == bSpareRepair.section) {
					return;
				}
				if (a_number->section != section) {
					section = a_number->section;
					std::format_to(std::back_inserter(line), " [{:s}]", section);
				}
				say(*a_number);
				for (const auto* after : SwitchesAfter(*a_number)) {
					std::format_to(std::back_inserter(line), " {:s}={:s}", after->key, Shown(*after));
				}
			}, entry);
		}
		std::format_to(std::back_inserter(line), " [Log] sLogLevel={:s} bTraceLogs={}", LogLevelWord(), bTraceLogs.GetValue());
		REX::INFO("{:s}", line);
		// Above info NEC.log drops these lines, so no summary is kept and the
		// next one at info is written in full.
		if (LogLevelNow() > REX::ELogLevel::Info) {
			CallPatch::KeepSummary({});
			return;
		}
		// Every summary written is kept, so the next one compares with it.
		const auto summary = CallPatch::Summary();
		if (CallPatch::KeepSummary(summary) || a_always) {
			for (const auto& text : summary) {
				REX::INFO("{:s}", text);
			}
		}
	}
}
