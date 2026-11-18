#include "UI/Hud/HudParts/Readout.h"

#include "Condition/Condition.h"
#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Hud/HudParts/HudParts.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// The texts of a place at Percent. Readout.cpp makes and writes the fields.
namespace HudParts
{
	namespace Readout
	{
		std::string PercentText(std::uint32_t a_tenths)
		{
			return Condition::CardText(a_tenths) + "%";
		}

		std::string WordAndPercent(Value& a_field, std::uint32_t a_tenths)
		{
			return Translated(a_field, CND_TEXT) + " " + PercentText(a_tenths);
		}

		LabelPlace ShowNumber(Value& a_readout, std::string_view a_text, const Label& a_label, Align a_align)
		{
			auto label = LabelOf(a_readout);
			ShowBar(a_readout, false);
			ShowLabel(a_readout, true);
			const auto written = WriteText(label, a_text, a_label.size, a_label.wide);
			const auto& letters = written.letters;
			const auto  from = a_align == Align::kStart ? letters.left : a_align == Align::kMiddle ? (letters.left + letters.right) / 2.0 : letters.right;
			MoveField(label, Flash::Number(label, "x"sv) + a_label.gap - from, a_label.drop - (Flash::Number(label, "height"sv) / 2.0));
			return ExtentOf(label);
		}

		void MoveField(Value& a_field, double a_x, double a_y)
		{
			Flash::Set(a_field, "x"sv, Value(a_x));
			Flash::Set(a_field, "y"sv, Value(a_y));
		}

		void TellForm(Settings::Cnd a_form, std::optional<Settings::Cnd>& a_said, std::string_view a_place)
		{
			if (a_said == a_form) {
				return;
			}
			const auto first = !a_said;
			a_said = a_form;
			if (!first) {
				TraceLog::Line("menu", "{:s} now shows {:s}", a_place,
					a_form == Settings::Cnd::kPercent ? "a percent" : a_form == Settings::Cnd::kBar ? "a bar" : "nothing");
			}
		}
	}
}
