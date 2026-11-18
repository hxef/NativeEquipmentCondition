#include "UI/Hud/HudParts/Readout.h"

#include "Condition/Condition.h"
#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Hud/HudParts/HudParts.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace HudParts
{
	namespace Readout
	{
		namespace
		{
			// The parts of every readout. Each readout is a sprite of its own,
			// so all of them use the same 7 names. The 4 in the middle draw
			// the mark where penalties start, see SetPercent.
			constexpr const char* TRACK_NAME = "NEC_ConditionTrack_mc";
			constexpr const char* FILL_NAME = "NEC_ConditionFill_mc";
			constexpr const char* TRACK_PAST_NAME = "NEC_ConditionTrackPast_mc";
			constexpr const char* FILL_PAST_NAME = "NEC_ConditionFillPast_mc";
			constexpr const char* NOTCHES_NAME = "NEC_ConditionNotches_mc";
			constexpr const char* LINE_NAME = "NEC_ConditionLine_mc";
			constexpr const char* LABEL_NAME = "NEC_ConditionLabel_tf";

			// The parts only a place at Percent makes, see Extra.
			constexpr const char* WORD_NAME = "NEC_ConditionWord_tf";
			constexpr const char* NUMBER_NAME = "NEC_ConditionNumber_tf";
			constexpr const char* RULE_NAME = "NEC_ConditionRule_mc";

			[[nodiscard]] const char* NameOf(Extra a_extra)
			{
				switch (a_extra) {
				case Extra::kWord:
					return WORD_NAME;
				case Extra::kNumber:
					return NUMBER_NAME;
				default:
					return RULE_NAME;
				}
			}

			// The HP meter draws its spare bar at half opacity, so the track
			// does too.
			constexpr double TRACK_ALPHA = 0.5;

			// Each notch reaches 0.4 of the bar's depth in, so a fifth of the
			// bar runs between their tips.
			constexpr double NOTCH_DEEP = 0.4;

			// The fill is the white at 1 and the track at 0.5, so the line at
			// 0.75 is always halfway between them on whatever is behind the bar.
			constexpr double LINE_ALPHA = (1.0 + TRACK_ALPHA) / 2.0;

			// How far the fill's end keeps out of the mark near the point, as a
			// share of the bar's normal depth.
			constexpr double STEP_SHARE = 1.0 / 6.0;

			// The whole size the word is written at, see SetStyledText. Its
			// field is then scaled to the size asked for.
			constexpr std::int32_t WRITTEN_SIZE = 20;

			// Adds a box of the mark, hidden and 0 wide at the readout's
			// origin until SetPercent places it.
			bool AddMarkBox(Scaleform::GFx::Movie& a_movie, Value& a_readout, const char* a_name, double a_alpha, bool a_notches)
			{
				const auto added = a_notches ? AddNotches(a_movie, a_readout, a_name, NOTCH_DEEP) : AddBox(a_movie, a_readout, a_name);
				auto       box = Flash::Child(a_readout, a_name);
				if (!added || !box.IsDisplayObject()) {
					return false;
				}
				PutBox(box, 0.0, 0.0, 0.0, 0.0);
				box.SetMember("alpha"sv, Value(a_alpha));
				box.SetMember("visible"sv, Value(false));
				return true;
			}
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

			// The track goes down first, the bar over it and the mark over
			// both.
			if (!AddBox(a_movie, readout, TRACK_NAME) ||
				!AddBox(a_movie, readout, FILL_NAME) ||
				!AddMarkBox(a_movie, readout, TRACK_PAST_NAME, TRACK_ALPHA, false) ||
				!AddMarkBox(a_movie, readout, FILL_PAST_NAME, 1.0, false) ||
				!AddMarkBox(a_movie, readout, NOTCHES_NAME, 1.0, true) ||
				!AddMarkBox(a_movie, readout, LINE_NAME, LINE_ALPHA, false) ||
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
			return ExtentOf(label);
		}

		LabelPlace ExtentOf(Value& a_field)
		{
			if (!a_field.IsDisplayObject()) {
				return LabelPlace{};
			}

			// The field is made from code, so its box starts at its own 0,0.
			const auto       left = Flash::Number(a_field, "x"sv);
			const auto       top = Flash::Number(a_field, "y"sv);
			const auto       across = Flash::Number(a_field, "scaleX"sv);
			const auto       down = Flash::Number(a_field, "scaleY"sv);
			const Flash::Box field{ left, top, left + Flash::Number(a_field, "width"sv), top + Flash::Number(a_field, "height"sv) };
			if (const auto line = Flash::FirstLine(a_field)) {
				return LabelPlace{ field, { left + (line->left * across), top + (line->top * down), left + (line->right * across), top + (line->baseline * down) }, true,
					line->descent * down };
			}
			const auto margin = Flash::FIELD_MARGIN;
			return LabelPlace{ field, { field.left + (margin * across), field.top + (margin * down), field.right - (margin * across), field.bottom - (margin * down) } };
		}

		void MoveLabel(Value& a_readout, double a_x, double a_y)
		{
			auto label = LabelOf(a_readout);
			if (label.IsDisplayObject()) {
				MoveField(label, a_x, a_y);
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

		Still StillEnd(Still a_shipped, bool a_flipped)
		{
			if (!a_flipped) {
				return a_shipped;
			}
			return a_shipped == Still::kRightEnd ? Still::kLeftEnd : Still::kRightEnd;
		}

		void SetPercent(Value& a_readout, double a_width, double a_deep, std::int32_t a_percent, Still a_still, const Mark& a_mark)
		{
			auto fill = Flash::Child(a_readout, FILL_NAME);
			if (!fill.IsDisplayObject()) {
				return;
			}
			auto track = Flash::Child(a_readout, TRACK_NAME);
			auto trackPast = Flash::Child(a_readout, TRACK_PAST_NAME);
			auto fillPast = Flash::Child(a_readout, FILL_PAST_NAME);
			auto notches = Flash::Child(a_readout, NOTCHES_NAME);
			auto line = Flash::Child(a_readout, LINE_NAME);

			// Every span is counted from the end that stays still, so on the
			// HUD 0 is the right end and on the Pip-Boy the left end.
			const auto place = [&](Value& a_box, double a_from, double a_to) {
				if (a_box.IsDisplayObject()) {
					const auto left = a_still == Still::kLeftEnd ? -a_width + a_from : -a_to;
					PutBox(a_box, left, -a_deep / 2.0, a_to - a_from, a_deep);
				}
			};
			const auto show = [&](Value& a_box, double a_from, double a_to) {
				place(a_box, a_from, a_to);
				if (a_box.IsDisplayObject()) {
					a_box.SetMember("visible"sv, Value(true));
				}
			};
			// A hidden child may still count in its parent's bounds, so a
			// hidden box sits 0 wide at the track's left end and the
			// readout's box never changes.
			const auto hide = [&](Value& a_box) {
				if (a_box.IsDisplayObject()) {
					PutBox(a_box, -a_width, -a_deep / 2.0, 0.0, a_deep);
					a_box.SetMember("visible"sv, Value(false));
				}
			};

			const auto filled = a_width * std::clamp(a_percent, 0, 100) / 100.0;
			if (a_mark.at == 0) {
				place(track, 0.0, a_width);
				place(fill, 0.0, filled);
				hide(trackPast);
				hide(fillPast);
				hide(notches);
				hide(line);
				return;
			}

			// The mark is centred on the point and kept inside the bar. Near
			// the point the fill's end keeps a_mark.step out of it, so over
			// and under read apart: at or over the point the fill runs past
			// the mark, under it the fill stops short of it.
			const auto wide = std::min(a_mark.wide, a_width);
			const auto from = std::clamp((a_width * a_mark.at / 100.0) - (wide / 2.0), 0.0, a_width - wide);
			const auto to = from + wide;
			const auto over = a_percent >= a_mark.at;
			const auto end = over ? std::min(a_width, std::max(filled, to + a_mark.step)) : std::max(0.0, std::min(filled, from - a_mark.step));

			if (!a_mark.line) {
				place(track, 0.0, a_width);
				place(fill, 0.0, end);
				show(notches, from, to);
				hide(trackPast);
				hide(fillPast);
				hide(line);
				return;
			}

			// The line splits the track and the fill in 2, since a box at
			// LINE_ALPHA laid over the fill would vanish into it.
			place(track, 0.0, from);
			show(trackPast, to, a_width);
			place(fill, 0.0, std::min(end, from));
			if (over) {
				show(fillPast, to, end);
			} else {
				hide(fillPast);
			}
			show(line, from, to);
			hide(notches);
		}

		void MirrorLabel(Value& a_readout, double a_width)
		{
			// The bar runs from -a_width to 0, so a field from x to x + width
			// mirrors to -a_width - (x + width).
			auto label = Flash::Child(a_readout, LABEL_NAME);
			if (label.IsDisplayObject()) {
				const auto x = Flash::Number(label, "x"sv);
				label.SetMember("x"sv, Value(-a_width - (x + Flash::Number(label, "width"sv))));
			}
		}

		void TellFlipped(bool a_flipped, bool& a_said, std::string_view a_bars)
		{
			if (a_flipped == a_said) {
				return;
			}
			a_said = a_flipped;
			TraceLog::Line("menu", "{:s} drawn {:s}", a_bars, a_flipped ? "flipped" : "as shipped");
		}

		double Held(const Settings::Live<float>& a_value, double a_least, double a_most)
		{
			const auto value = a_value.GetValue();
			return std::isfinite(value) ? std::clamp(static_cast<double>(value), a_least, a_most) : 1.0;
		}

		Size SizeFrom(const Settings::Live<float>& a_length, const Settings::Live<float>& a_thickness)
		{
			return Size{ Held(a_length, LEAST_SIZE, MOST_SIZE), Held(a_thickness, LEAST_SIZE, MOST_SIZE) };
		}

		Mark PenaltyMark(Condition::Kind a_kind, double a_deep, double a_wide)
		{
			const auto at = Condition::StartPercent(a_kind);
			if (at == 0) {
				return Mark{};
			}
			return Mark{ at, a_wide * Held(Settings::fPenaltyMarkWidth, MARK_LEAST, MARK_MOST), a_deep * STEP_SHARE, Settings::bPenaltyMarkAsLine.GetValue() };
		}

		// -------------------------------------------------------------------
		// A percent in place of the bar
		// -------------------------------------------------------------------

		Value ExtraIn(Value& a_readout, Extra a_extra)
		{
			return Flash::Child(a_readout, NameOf(a_extra));
		}

		Value ExtraOf(Scaleform::GFx::Movie& a_movie, Value& a_readout, Extra a_extra)
		{
			if (auto made = ExtraIn(a_readout, a_extra); made.IsDisplayObject()) {
				return made;
			}
			const auto* name = NameOf(a_extra);
			const auto  added = a_extra == Extra::kLine ? AddBox(a_movie, a_readout, name) : AddField(a_movie, a_readout, name);
			auto       made = added ? Flash::Child(a_readout, name) : Value{};
			if (!made.IsDisplayObject()) {
				return Value{};
			}
			made.SetMember("visible"sv, Value(false));
			if (a_extra != Extra::kLine) {
				const auto label = Flash::Child(a_readout, LABEL_NAME);
				if (const auto how = Flash::Member(label, "antiAliasType"sv); how.IsString()) {
					made.SetMember("antiAliasType"sv, how);
				}
			}
			return made;
		}

		Value LabelOf(Value& a_readout)
		{
			return Flash::Child(a_readout, LABEL_NAME);
		}

		void ShowBar(Value& a_readout, bool a_shown)
		{
			for (const auto* name : { TRACK_NAME, FILL_NAME }) {
				auto box = Flash::Child(a_readout, name);
				Flash::Set(box, "visible"sv, Value(a_shown));
			}
			if (a_shown) {
				return;
			}
			for (const auto* name : { TRACK_PAST_NAME, FILL_PAST_NAME, NOTCHES_NAME, LINE_NAME }) {
				auto box = Flash::Child(a_readout, name);
				Flash::Set(box, "visible"sv, Value(false));
			}
		}

		void ShowLabel(Value& a_readout, bool a_shown)
		{
			auto label = Flash::Child(a_readout, LABEL_NAME);
			Flash::Set(label, "visible"sv, Value(a_shown));
		}

		LabelPlace WriteText(Value& a_field, std::string_view a_text, double a_size, double a_wide)
		{
			if (!a_field.IsDisplayObject()) {
				return LabelPlace{};
			}
			const auto scale = a_size / WRITTEN_SIZE;
			SetStyledText(a_field, a_text, WRITTEN_SIZE);
			a_field.SetMember("scaleX"sv, Value(a_wide * scale));
			a_field.SetMember("scaleY"sv, Value(scale));
			return ExtentOf(a_field);
		}
	}
}
