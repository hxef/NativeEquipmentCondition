#include "UI/Mcm/Bridge.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Presets/Presets.h"
#include "Core/Settings/Settings.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <variant>

// The settings of MCM's UI page, which Reset UI settings puts back to their
// defaults, see Buttons.cpp. None is held by a preset, so each default is
// NEC.ini's value, and the line of each setting it changes goes from
// NEC_custom.ini.
namespace Mcm
{
	namespace
	{
		using Setting = std::variant<Settings::Live<bool>*, Settings::Live<std::int32_t>*, Settings::Live<float>*>;

		// Every row of the UI page that sets something, in config.json's
		// order. A row added to the page goes here too, or the reset skips it.
		const Setting UI_PAGE[]{
			&Settings::iWeaponCnd,
			&Settings::fHudBarX,
			&Settings::fHudBarY,
			&Settings::fHudBarLength,
			&Settings::fHudBarThickness,
			&Settings::bHudBarShowsWord,
			&Settings::bHudBarFlipped,
			&Settings::bHudBarPercentOneLine,
			&Settings::bHudBarHidesWithCounter,
			&Settings::bHudBarStandsAlone,
			&Settings::iPowerArmorCnd,
			&Settings::fPowerArmorBarX,
			&Settings::fPowerArmorBarY,
			&Settings::fPowerArmorBarLength,
			&Settings::fPowerArmorBarThickness,
			&Settings::bPowerArmorBarShowsWord,
			&Settings::bPowerArmorBarFlipped,
			&Settings::iPickupCnd,
			&Settings::fPickupBarX,
			&Settings::fPickupBarY,
			&Settings::fPickupBarLength,
			&Settings::fPickupBarThickness,
			&Settings::bPickupBarShowsWord,
			&Settings::bPickupBarFlipped,
			&Settings::bPickupPenaltyMark,
			&Settings::iQuickContainerCnd,
			&Settings::bQuickContainerBarFlipped,
			&Settings::bQuickContainerCndAtRight,
			&Settings::iItemCardCnd,
			&Settings::fItemCardBarThickness,
			&Settings::bItemCardBarFlipped,
			&Settings::bCardWearRate,
			&Settings::bPowerArmorCardCnd,
			&Settings::iCndDecimals,
			&Settings::bCndColumn,
			&Settings::iApparelFigureCnd,
			&Settings::fPipboyBarThickness,
			&Settings::bPipboyBarFlipped,
			&Settings::fPenaltyMarkWidth,
			&Settings::bPenaltyMarkAsLine,
		};

		// A value as NEC_custom.ini keeps it, as the page's own changes write it.
		std::string Word(bool a_value) { return a_value ? "true" : "false"; }
		std::string Word(std::int32_t a_value) { return std::format("{:d}", a_value); }
		std::string Word(float a_value) { return std::format("{:g}", a_value); }

		// Puts 1 setting back as a change from the page, so it is written and
		// logged as one. A switch or stepper left to another mod stays off, as
		// the page leaves it, see SetBool in Bridge.cpp. True when it changed.
		template <class T>
		bool PutBack(Settings::Live<T>& a_setting)
		{
			if (CallPatch::IsYielded(a_setting)) {
				return false;
			}
			const auto before = a_setting.GetValue();
			const auto value = a_setting.GetValueDefault();
			if (before == value) {
				return false;
			}
			a_setting.SetValue(value);
			Owe(a_setting.section, a_setting.key, Word(before), Word(value), Presets::Base(a_setting));
			Followed(a_setting);
			return true;
		}
	}

	std::size_t ResetUiPage()
	{
		std::size_t changed = 0;
		for (const auto& setting : UI_PAGE) {
			std::visit([&](auto* a_setting) {
				if (PutBack(*a_setting)) {
					++changed;
				}
			}, setting);
		}
		return changed;
	}
}
