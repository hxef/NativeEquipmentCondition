#include "UI/Hud/HudParts/HudParts.h"

#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "UI/Flash.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace HudParts
{
	namespace Readout
	{
		namespace
		{
			// The parts of every readout. Each readout is a sprite of its own,
			// so all of them use the same 3 names.
			constexpr const char* TRACK_NAME = "NEC_ConditionTrack_mc";
			constexpr const char* FILL_NAME = "NEC_ConditionFill_mc";
			constexpr const char* LABEL_NAME = "NEC_ConditionLabel_tf";

			// The HP meter draws its spare bar at half opacity, so the track
			// does too.
			constexpr double TRACK_ALPHA = 0.5;

			// The whole size the word is written at, see SetStyledText. Its
			// field is then scaled to the size asked for.
			constexpr std::int32_t WRITTEN_SIZE = 20;
		}

		Value Create(Scaleform::GFx::Movie& a_movie, const char* a_name)
		{
			Value readout;
			a_movie.CreateObject(&readout, "flash.display.Sprite");
			if (!readout.IsDisplayObject()) {
				return Value{};
			}
			readout.SetMember("name"sv, Value(a_name));
			readout.SetMember("mouseEnabled"sv, Value(false));
			readout.SetMember("mouseChildren"sv, Value(false));
			readout.SetMember("visible"sv, Value(false));

			// The track goes down first and the bar over it.
			if (!AddBox(a_movie, readout, TRACK_NAME) ||
				!AddBox(a_movie, readout, FILL_NAME) ||
				!AddField(a_movie, readout, LABEL_NAME)) {
				return Value{};
			}
			return readout;
		}

		void AntiAliasForAnimation(Value& a_readout)
		{
			// ActionScript's "normal" is anti-alias for animation, "advanced"
			// for readability.
			auto label = Flash::Child(a_readout, LABEL_NAME);
			if (label.IsDisplayObject()) {
				label.SetMember("antiAliasType"sv, Value("normal"));
			}
		}

		double SetLabel(Value& a_readout, const Label& a_label)
		{
			auto label = Flash::Child(a_readout, LABEL_NAME);
			if (!label.IsDisplayObject()) {
				return 0.0;
			}

			// The field sizes itself to its text, so its size is known once
			// the text is in. It keeps a margin on each side, scaled with its
			// letters, and its middle is the middle of the capitals, so a word
			// set level with the bar looks level.
			const auto scale = a_label.size / WRITTEN_SIZE;
			SetTranslatedText(label, CND_TEXT, WRITTEN_SIZE);
			label.SetMember("scaleX"sv, Value(a_label.wide * scale));
			label.SetMember("scaleY"sv, Value(scale));
			const auto margin = Flash::FIELD_MARGIN * a_label.wide * scale;
			label.SetMember("x"sv, Value(a_label.gap - margin));
			label.SetMember("y"sv, Value(a_label.drop - (Flash::Number(label, "height"sv) / 2.0)));
			return a_label.gap + Flash::Number(label, "width"sv) - (margin * 2.0);
		}

		LabelPlace LabelExtent(Value& a_readout)
		{
			auto label = Flash::Child(a_readout, LABEL_NAME);
			if (!label.IsDisplayObject()) {
				return LabelPlace{};
			}

			// The field is made from code, so its box starts at its own 0,0.
			const auto       left = Flash::Number(label, "x"sv);
			const auto       top = Flash::Number(label, "y"sv);
			const auto       across = Flash::Number(label, "scaleX"sv);
			const auto       down = Flash::Number(label, "scaleY"sv);
			const Flash::Box field{ left, top, left + Flash::Number(label, "width"sv), top + Flash::Number(label, "height"sv) };
			if (const auto line = Flash::FirstLine(label)) {
				return LabelPlace{ field, { left + (line->left * across), top + (line->top * down), left + (line->right * across), top + (line->baseline * down) }, true,
					line->descent * down };
			}
			const auto margin = Flash::FIELD_MARGIN;
			return LabelPlace{ field, { field.left + (margin * across), field.top + (margin * down), field.right - (margin * across), field.bottom - (margin * down) } };
		}

		void MoveLabel(Value& a_readout, double a_x, double a_y)
		{
			auto label = Flash::Child(a_readout, LABEL_NAME);
			if (label.IsDisplayObject()) {
				label.SetMember("x"sv, Value(a_x));
				label.SetMember("y"sv, Value(a_y));
			}
		}

		void FollowWordSwitch(Value& a_readout, const Settings::Live<bool>& a_shows, std::optional<bool>& a_shown, std::string_view a_bar)
		{
			const auto shown = a_shows.GetValue();
			if (a_shown == shown) {
				return;
			}
			auto label = Flash::Child(a_readout, LABEL_NAME);
			if (!label.IsDisplayObject()) {
				return;
			}
			label.SetMember("visible"sv, Value(shown));
			a_shown = shown;
			TraceLog::Line("menu", "{:s} CND word {:s}", a_bar, shown ? "shows" : "hides, the bar stays");
		}

		void SetTrack(Value& a_readout, double a_width, double a_deep)
		{
			auto track = Flash::Child(a_readout, TRACK_NAME);
			if (!track.IsDisplayObject()) {
				return;
			}

			PutBox(track, -a_width, -a_deep / 2.0, a_width, a_deep);
			track.SetMember("alpha"sv, Value(TRACK_ALPHA));
		}

		void SetPercent(Value& a_readout, double a_width, double a_deep, std::int32_t a_percent, Still a_still)
		{
			auto fill = Flash::Child(a_readout, FILL_NAME);
			if (!fill.IsDisplayObject()) {
				return;
			}

			const auto filled = a_width * std::clamp(a_percent, 0, 100) / 100.0;
			const auto left = a_still == Still::kLeftEnd ? -a_width : -filled;
			PutBox(fill, left, -a_deep / 2.0, filled, a_deep);
		}

		Size SizeFrom(const Settings::Live<float>& a_length, const Settings::Live<float>& a_thickness)
		{
			const auto held = [](float a_value) { return std::isfinite(a_value) ? std::clamp(static_cast<double>(a_value), LEAST_SIZE, MOST_SIZE) : 1.0; };
			return Size{ held(a_length.GetValue()), held(a_thickness.GetValue()) };
		}
	}
}
