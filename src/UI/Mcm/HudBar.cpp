#include "UI/Mcm/Bridge.h"

#include "Core/Settings.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Hud/HudCondition/HudCondition.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

// The HUD bar sliders. NEC.ini keeps how far the bar moved from its spot in
// HUD units, see HudCondition.h. The page shows where the bar and its word's
// letters start instead, also in HUD units, from the left and top edges of
// the visible frame, rounded to the rows' step. The game stretches the HUD
// over the part of the screen it draws in, so that frame and a number's
// place stay the same when the screen size changes. Each slider runs from 0
// to the frame's width or height less the bar, so the HUD never has to hold
// a value the page sets. MCM copies a slider's ends from config.json once a
// visit, so Layout.cpp writes the live ones. config.json's 0 to 1280 and 0
// to 720 serve only an MCM whose list NEC cannot read, and HudBarShift still
// keeps a value on screen there. The number the help names as the default
// puts NEC.ini's value back exactly.
namespace Mcm
{
	namespace
	{
		// config.json's step for the 2 rows. Change both together, or the
		// shown number falls between MCM's steps.
		constexpr double STEP = 5.0;

		// A number within half a HUD unit of the default is the default.
		constexpr double SAME_UNIT = 0.5;

		// MCM keeps its numbers as doubles, so a member within a 1000th of
		// the number wanted is not written again.
		constexpr double SAME_NUMBER = 0.001;

		// The hidden switch the 2 rows hide behind until the HUD has placed
		// the bar once.
		constexpr std::string_view PLACED_ID = "bHudCondition:Placed"sv;

		constexpr std::array SETTINGS{ &Settings::fHudBarX, &Settings::fHudBarY };

		// A slider, across or down, in HUD units from the frame's left or top
		// edge. own is where the box's left or top edge sits on its spot, and
		// to how far that edge goes with the box on screen. sign is 1, or -1
		// down, since fHudBarY above 0 is up. high is the slider's far end,
		// on the step, and its near end is 0.
		struct Axis
		{
			double own = 0.0;
			double to = 0.0;
			double sign = 0.0;
			double high = 0.0;
		};

		// A slider with the HUD's last measure, and the setting's value as
		// the HUD reads it.
		struct Slider
		{
			const Settings::Live<float>* setting = nullptr;
			Axis                         axis;
			double                       stored = 0.0;
		};

		[[nodiscard]] Axis AxisOf(const HudCondition::Measure& a_measure, bool a_down)
		{
			const auto& [box, frame] = a_measure;
			const auto own = a_down ? box.top - frame.top : box.left - frame.left;
			const auto to = a_down ? (frame.bottom - frame.top) - (box.bottom - box.top) : (frame.right - frame.left) - (box.right - box.left);

			// 1 step at least, so MCM never divides by 0.
			return Axis{ own, to, a_down ? -1.0 : 1.0, std::max(STEP, std::floor(to / STEP) * STEP) };
		}

		// Where the box's edge sits for a stored value, held as the HUD holds
		// it. to is already the far edge less the box, so the edge is held as
		// a box 0 wide.
		[[nodiscard]] double Real(const Axis& a_axis, double a_stored)
		{
			return a_axis.own + HudCondition::Held(a_stored * a_axis.sign, a_axis.own, a_axis.own, 0.0, a_axis.to);
		}

		// What the slider shows for a stored value, on the step and within its
		// ends, as MCM would draw it anyway.
		[[nodiscard]] double Shown(const Axis& a_axis, double a_stored)
		{
			return std::clamp(std::round(Real(a_axis, a_stored) / STEP) * STEP, 0.0, a_axis.high);
		}

		[[nodiscard]] std::optional<Slider> SliderOf(const Settings::Live<float>* a_setting)
		{
			const auto measure = a_setting ? HudCondition::Measured() : std::nullopt;
			if (!measure) {
				return std::nullopt;
			}
			const auto down = a_setting == &Settings::fHudBarY;
			const auto shift = HudCondition::ShiftNow();
			return Slider{ a_setting, AxisOf(*measure, down), down ? shift.y : shift.x };
		}

		// A slider by its id, fHudBarX:HUD.
		[[nodiscard]] const Settings::Live<float>* SettingOf(std::string_view a_id)
		{
			const auto it = std::ranges::find_if(SETTINGS, [&](const auto* a_setting) { return IsIdOf(*a_setting, a_id); });
			return it != SETTINGS.end() ? *it : nullptr;
		}

		// What the slider shows at NEC.ini's value, read as the HUD reads it.
		[[nodiscard]] double DefaultOf(const Slider& a_slider)
		{
			return Shown(a_slider.axis, HudCondition::Finite(a_slider.setting->GetValueDefault()));
		}
	}

	bool IsHudBar(std::string_view a_id)
	{
		return SettingOf(a_id) != nullptr;
	}

	bool HudBarHidden(const Settings::Named& a_setting)
	{
		return std::ranges::contains(SETTINGS, &a_setting) && !HudCondition::Measured();
	}

	std::optional<bool> HudBarShows(std::string_view a_id)
	{
		return a_id == PLACED_ID ? std::optional{ HudCondition::Measured().has_value() } : std::nullopt;
	}

	std::optional<double> HudBarValue(std::string_view a_id)
	{
		const auto slider = SliderOf(SettingOf(a_id));
		return slider ? std::optional{ Shown(slider->axis, slider->stored) } : std::nullopt;
	}

	// A number past an end counts as the end, whoever sent it.
	std::optional<float> HudBarShift(std::string_view a_id, float a_number)
	{
		const auto slider = SliderOf(SettingOf(a_id));
		if (!slider) {
			return std::nullopt;
		}
		const auto& axis = slider->axis;
		const auto  at = std::clamp(static_cast<double>(a_number), 0.0, axis.high);

		// The default's number stores NEC.ini's value exactly, so
		// NEC_custom.ini drops the key.
		if (std::abs(at - DefaultOf(*slider)) < SAME_UNIT) {
			return slider->setting->GetValueDefault();
		}
		return static_cast<float>((at - axis.own) * axis.sign);
	}

	std::optional<std::int32_t> HudBarDefault(std::string_view a_id)
	{
		if (!a_id.ends_with(Text::MENU_HELP)) {
			return std::nullopt;
		}
		const auto key = a_id.substr(0, a_id.size() - Text::MENU_HELP.size());
		const auto it = std::ranges::find(SETTINGS, key, [](const auto* a_setting) { return a_setting->key; });
		const auto slider = it != SETTINGS.end() ? SliderOf(*it) : std::nullopt;
		return slider ? std::optional{ static_cast<std::int32_t>(std::lround(DefaultOf(*slider))) } : std::nullopt;
	}

	bool HudBarRanges(Scaleform::GFx::Value& a_entries)
	{
		auto       wrote = false;
		const auto count = a_entries.GetArraySize();
		for (std::uint32_t i = 0; i < count; i++) {
			Scaleform::GFx::Value entry;
			Scaleform::GFx::Value id;
			if (!a_entries.GetElement(i, &entry) || !entry.IsObject() || !entry.GetMember("id"sv, &id) || !id.IsString() || !id.GetString()) {
				continue;
			}
			const auto slider = SliderOf(SettingOf(id.GetString()));
			if (!slider) {
				continue;
			}

			const auto& axis = slider->axis;
			const auto  value = Shown(axis, slider->stored);
			auto        changed = false;
			for (const auto& [name, want] : { std::pair{ "minvalue"sv, 0.0 }, std::pair{ "maxvalue"sv, axis.high }, std::pair{ "value"sv, value } }) {
				// A missing member reads as nan, which is never near.
				if (!(std::abs(Flash::Number(entry, name, std::numeric_limits<double>::quiet_NaN()) - want) <= SAME_NUMBER)) {
					entry.SetMember(name, Scaleform::GFx::Value(want));
					changed = true;
				}
			}
			if (changed) {
				wrote = true;
				TraceLog::Line("menu", "MCM slider {:s} runs 0 to {:.0f} at {:.0f}", id.GetString(), axis.high, value);
			}
		}
		return wrote;
	}
}
