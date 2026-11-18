#include "Core/Settings/Settings.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Presets/Presets.h"
#include "Core/Runtime.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <iterator>
#include <optional>
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

		// The switches and CND steppers that lead [Features] on the Settings
		// line. Every one a row of Features.cpp turns on with is here, so the
		// walks of what is left to other mods find it.
		constexpr Gate GATES[]{
			&bSpawnCondition,
			&bWorkbenchRepair,
			&bVendorRepair,
			&bLoadingTips,
			&iWeaponCnd,
			&iQuickContainerCnd,
			&bConfirmScroll,
			&bInspectPrice,
			&bFoodSpoils,
		};

		const Number NUMBERS[]{
			&iPipboyRepair,
			&iUnscrappableSpares,
			&iKeepSpareMods,
			&iSpareModRows,
			&iRepairBoxRow,
			&iRepairLock,
			&iWeaponCnd,
			&iPowerArmorCnd,
			&iQuickContainerCnd,
			&iItemCardCnd,
			&iCndDecimals,
			&iLootShift,
			&iLootSurpriseOdds,
			&fLootAreaEffect,
			&fWearRateMult,
			&fNpcWearRateMult,
			&fAutoWearRateMult,
			&fArmorWearRateMult,
			&fNpcArmorWearRateMult,
			&fCompanionWearRateMult,
			&fSettlerWearRateMult,
			&fCompanionArmorWearRateMult,
			&fSettlerArmorWearRateMult,
			&fSuppressedWearRateMult,
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
			&iPerkComponentDiscount,
			&fSpareWorthMult,
			&fFreeMendFrom,
			&iRepairSupplies,
			&fSuppliesOnlyFrom,
			&iModdingFrom,
			&iPerkLock,
			&fTraderPriceMult,
			&iTraderLeeway,
			&iTraderFullStock,
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
			&iPickupCnd,
			&fPickupBarX,
			&fPickupBarY,
			&fPickupBarLength,
			&fPickupBarThickness,
			&fItemCardBarThickness,
			&iApparelFigureCnd,
			&fPipboyBarThickness,
			&fPenaltyMarkWidth,
		};

		// Every switch in NEC.ini's order, for a lookup by id.
		Live<bool>* const EVERY_SWITCH[]{
			&bNecOn,
			&bPlainApparelNeverWears,
			&bSpawnCondition,
			&bWorkbenchRepair,
			&bVendorRepair,
			&bTraderSign,
			&bSpareRepair,
			&bLockSpares,
			&bRepairBoxColumns,
			&bRepairBoxGains,
			&bLoadingTips,
			&bCardWearRate,
			&bPowerArmorCardCnd,
			&bCndColumn,
			&bConfirmScroll,
			&bInspectPrice,
			&bFoodSpoils,
			&bFridgeIcon,
			&bHudBarHidesWithCounter,
			&bHudBarStandsAlone,
			&bHudBarShowsWord,
			&bHudBarFlipped,
			&bPowerArmorBarShowsWord,
			&bPowerArmorBarFlipped,
			&bPickupBarShowsWord,
			&bPickupBarFlipped,
			&bPickupPenaltyMark,
			&bQuickContainerBarFlipped,
			&bItemCardBarFlipped,
			&bPipboyBarFlipped,
			&bPenaltyMarkAsLine,
			&bTraceLogs,
		};

		// The switches GATES leaves out, each after the setting NEC.ini has
		// it after, for the Settings line. bNecOn and bPlainApparelNeverWears
		// come first in [Features], so ReportLine says them before the list.
		Live<bool>* const AFTER_VENDOR_REPAIR[]{ &bTraderSign, &bSpareRepair, &bLockSpares, &bRepairBoxColumns, &bRepairBoxGains };
		Live<bool>* const AFTER_ITEM_CARD_CND[]{ &bCardWearRate, &bPowerArmorCardCnd };
		Live<bool>* const AFTER_FOOD_SPOILS[]{ &bFridgeIcon };
		Live<bool>* const AFTER_HUD_BAR[]{ &bHudBarHidesWithCounter, &bHudBarStandsAlone, &bHudBarShowsWord, &bHudBarFlipped };
		Live<bool>* const AFTER_POWER_ARMOR_BAR[]{ &bPowerArmorBarShowsWord, &bPowerArmorBarFlipped };
		Live<bool>* const AFTER_PICKUP_BAR[]{ &bPickupBarShowsWord, &bPickupBarFlipped, &bPickupPenaltyMark, &bQuickContainerBarFlipped };
		Live<bool>* const AFTER_ITEM_CARD_BAR[]{ &bItemCardBarFlipped };
		Live<bool>* const AFTER_PIPBOY_BAR[]{ &bPipboyBarFlipped };
		Live<bool>* const AFTER_PENALTY_MARK[]{ &bPenaltyMarkAsLine };
		Live<bool>* const AFTER_CND_DECIMALS[]{ &bCndColumn };

		// The numbers under [Features] GATES leaves out, which the Settings
		// line puts after the setting NEC.ini has them after.
		Live<std::int32_t>* const AFTER_SPARE_REPAIR[]{ &iPipboyRepair, &iUnscrappableSpares, &iKeepSpareMods, &iSpareModRows, &iRepairBoxRow, &iRepairLock };
		Live<std::int32_t>* const AFTER_WEAPON_CND[]{ &iPowerArmorCnd };
		Live<std::int32_t>* const AFTER_QUICK_CONTAINER_CND[]{ &iItemCardCnd };
		Live<std::int32_t>* const AFTER_POWER_ARMOR_CARD_CND[]{ &iCndDecimals };

		[[nodiscard]] std::span<Live<std::int32_t>* const> NumbersAfter(const Named& a_setting)
		{
			if (&a_setting == &bSpareRepair) {
				return AFTER_SPARE_REPAIR;
			}
			if (&a_setting == &iWeaponCnd) {
				return AFTER_WEAPON_CND;
			}
			if (&a_setting == &iQuickContainerCnd) {
				return AFTER_QUICK_CONTAINER_CND;
			}
			if (&a_setting == &bPowerArmorCardCnd) {
				return AFTER_POWER_ARMOR_CARD_CND;
			}
			return {};
		}

		[[nodiscard]] std::span<Live<bool>* const> SwitchesAfter(const Named& a_setting)
		{
			if (&a_setting == &bVendorRepair) {
				return AFTER_VENDOR_REPAIR;
			}
			if (&a_setting == &iItemCardCnd) {
				return AFTER_ITEM_CARD_CND;
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
			if (&a_setting == &fItemCardBarThickness) {
				return AFTER_ITEM_CARD_BAR;
			}
			if (&a_setting == &fPipboyBarThickness) {
				return AFTER_PIPBOY_BAR;
			}
			if (&a_setting == &fPenaltyMarkWidth) {
				return AFTER_PENALTY_MARK;
			}
			if (&a_setting == &iCndDecimals) {
				return AFTER_CND_DECIMALS;
			}
			return {};
		}

		// -------------------------------------------------------------------
		// How the line shows a setting
		// -------------------------------------------------------------------

		// Who has a setting's part, short for the Settings line: the DLLs,
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

		// Who has a switch or a CND stepper left to another mod, as Owned
		// says it, or nothing while it is free.
		std::optional<std::string> LeftTo(const Named& a_setting)
		{
			const auto yield = CallPatch::YieldOf(a_setting);
			if (!yield) {
				return std::nullopt;
			}
			std::vector<CallPatch::Owner> owners;
			for (const auto& cause : yield->causes) {
				for (const auto& owner : cause.owners) {
					if (std::ranges::find(owners, owner) == owners.end()) {
						owners.push_back(owner);
					}
				}
			}
			std::ranges::sort(owners, CallPatch::Before);
			return Owned(owners);
		}

		// A setting's value as the Settings line prints it. A switch or a
		// stepper left to another mod says to whom. Otherwise a switch that
		// is on, and any number, carries its mark.
		std::string Shown(const Live<bool>& a_switch)
		{
			if (const auto owners = LeftTo(a_switch)) {
				return std::format("false({:s})", *owners);
			}
			return a_switch.GetValue() ? std::format("true{:s}", Mark(a_switch)) : "false";
		}

		std::string Shown(const Live<float>& a_number)
		{
			return std::format("{:g}{:s}", a_number.GetValue(), Mark(a_number));
		}

		std::string Shown(const Live<std::int32_t>& a_number)
		{
			if (const auto owners = LeftTo(a_number)) {
				return std::format("{:d}({:s})", a_number.GetValue(), *owners);
			}
			return std::format("{:d}{:s}", a_number.GetValue(), Mark(a_number));
		}

		void SayAfter(std::string& a_line, const Named& a_setting);

		// A setting on the Settings line, then the settings NEC.ini has after
		// it, each with its own after it.
		template <class T>
		void Say(std::string& a_line, const Live<T>& a_setting)
		{
			std::format_to(std::back_inserter(a_line), " {:s}={:s}", a_setting.key, Shown(a_setting));
			SayAfter(a_line, a_setting);
		}

		void SayAfter(std::string& a_line, const Named& a_setting)
		{
			for (const auto* after : SwitchesAfter(a_setting)) {
				Say(a_line, *after);
			}
			for (const auto* after : NumbersAfter(a_setting)) {
				Say(a_line, *after);
			}
		}
	}

	std::span<const Gate> Gates()
	{
		return GATES;
	}

	std::span<const Number> Numbers()
	{
		return NUMBERS;
	}

	std::span<Live<bool>* const> EverySwitch()
	{
		return EVERY_SWITCH;
	}

	// -------------------------------------------------------------------
	// The Settings line
	// -------------------------------------------------------------------

	void ReportLine(bool a_always)
	{
		// Every key as it stands now, before any limit, in NEC.ini's order and
		// spelling.
		std::string line = std::format("Settings: [Features] {:s}={:s} {:s}={:s}", bNecOn.key, Shown(bNecOn), bPlainApparelNeverWears.key, Shown(bPlainApparelNeverWears));
		for (const auto& gate : GATES) {
			if (const auto* on = gate.Switch()) {
				Say(line, *on);
			} else {
				Say(line, *gate.Stepper());
			}
		}
		std::string_view section;
		for (const auto& entry : NUMBERS) {
			std::visit([&](const auto* a_number) {
				// Said after its switch or stepper above.
				if (a_number->section == bSpareRepair.section) {
					return;
				}
				if (a_number->section != section) {
					section = a_number->section;
					std::format_to(std::back_inserter(line), " [{:s}]", section);
				}
				Say(line, *a_number);
			}, entry);
		}
		std::format_to(std::back_inserter(line), " [Log] sLogLevel={:s} bTraceLogs={}", LogLevelWord(), bTraceLogs.GetValue());
		std::format_to(std::back_inserter(line), " [Presets] sPreset={:s}", Presets::PickedStem());
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
