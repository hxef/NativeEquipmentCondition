#include "UI/Hud/PowerArmorCondition/Lines.h"

#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Hud/HudParts/Readout.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

namespace PowerArmorCondition
{
	namespace
	{
		namespace Readout = HudParts::Readout;
		using Value = HudParts::Value;

		// a_place with its field's top left corner at 0,0.
		[[nodiscard]] Readout::LabelPlace FromCorner(const Readout::LabelPlace& a_place)
		{
			auto moved = a_place;
			moved.field = a_place.field.Moved(-a_place.field.left, -a_place.field.top);
			moved.letters = a_place.letters.Moved(-a_place.field.left, -a_place.field.top);
			return moved;
		}

		// A point in the movie's units in the readout's own, the readout
		// standing at a_origin turned a_turn radians clockwise, as Flash
		// turns a clip.
		[[nodiscard]] Flash::Point Inside(const Point& a_point, const Point& a_origin, double a_turn)
		{
			const auto x = a_point.x - a_origin.x;
			const auto y = a_point.y - a_origin.y;
			return Flash::Point{ (x * std::cos(a_turn)) + (y * std::sin(a_turn)), (y * std::cos(a_turn)) - (x * std::sin(a_turn)) };
		}

		// The other way round.
		[[nodiscard]] Point Outside(const Flash::Point& a_point, const Point& a_origin, double a_turn)
		{
			return Point{ a_origin.x + (a_point.x * std::cos(a_turn)) - (a_point.y * std::sin(a_turn)),
				a_origin.y + (a_point.x * std::sin(a_turn)) + (a_point.y * std::cos(a_turn)), true };
		}

		// Puts a field written as a_at says with its letters' left end and
		// its middle on a_point, in the movie's units, and returns whether
		// its 4 corners stand in a_frame. The field's middle is its
		// capitals' middle, as SetLabel sets the word level with a bar.
		bool PutOn(Value& a_field, const Readout::LabelPlace& a_at, const Point& a_point, const Frame& a_frame, const Point& a_origin, double a_turn)
		{
			const auto at = Inside(a_point, a_origin, a_turn);
			const auto left = at.x - a_at.letters.left;
			const auto top = at.y - (a_at.field.bottom / 2.0);
			Readout::MoveField(a_field, left, top);
			const auto box = a_at.field.Moved(left, top);
			const std::array corners{ Flash::Point{ box.left, box.top }, Flash::Point{ box.right, box.top }, Flash::Point{ box.left, box.bottom },
				Flash::Point{ box.right, box.bottom } };
			for (const auto& corner : corners) {
				if (!a_frame.Holds(Outside(corner, a_origin, a_turn))) {
					return false;
				}
			}
			return true;
		}
	}

	bool Lines::Lay(Scaleform::GFx::Movie& a_movie, Value& a_readout, const Box& a_box)
	{
		auto upper = Readout::ExtraOf(a_movie, a_readout, Readout::Extra::kWord);
		auto lower = Readout::ExtraOf(a_movie, a_readout, Readout::Extra::kNumber);
		if (!upper.IsDisplayObject() || !lower.IsDisplayObject()) {
			return false;
		}
		if (word.empty()) {
			word = HudParts::Translated(upper, HudParts::CND_TEXT);
		}
		size = a_box.row * LINES_SIZE;
		wordAt = FromCorner(Readout::WriteText(upper, word, size, LINES_WIDE));
		number.clear();
		return true;
	}

	std::optional<std::string> Lines::Write(Scaleform::GFx::Movie& a_movie, Value& a_readout, std::uint32_t a_tenths, bool a_worded)
	{
		auto       lower = Readout::ExtraOf(a_movie, a_readout, Readout::Extra::kNumber);
		const auto text = Readout::PercentText(a_tenths);
		if (text == number || !lower.IsDisplayObject() || !(size > 0.0)) {
			return std::nullopt;
		}
		numberAt = FromCorner(Readout::WriteText(lower, text, size, LINES_WIDE));
		number = text;
		return a_worded ? word + " " + text : text;
	}

	bool Lines::Place(Value& a_readout, const Frame& a_frame, const Point& a_origin, double a_turn, const Point& a_upper, const Point& a_lower, bool a_worded)
	{
		auto upper = Readout::ExtraIn(a_readout, Readout::Extra::kWord);
		auto lower = Readout::ExtraIn(a_readout, Readout::Extra::kNumber);
		// Before any early return, so CND never shows on a frame that places
		// nothing while it fades.
		if (worded != a_worded) {
			worded = a_worded;
			Flash::Set(upper, "visible"sv, Value(a_worded));
			TraceLog::Line("menu", "Power armor CND word {:s}", a_worded ? "shows over the percent" : "hides, the percent stands alone between the 2 rows");
		}
		if (!a_upper.ok || !a_lower.ok || !(size > 0.0) || number.empty()) {
			return false;
		}
		if (!a_worded) {
			const Point middle{ (a_upper.x + a_lower.x) / 2.0, (a_upper.y + a_lower.y) / 2.0, true };
			return PutOn(lower, numberAt, middle, a_frame, a_origin, a_turn);
		}
		const auto wordFits = PutOn(upper, wordAt, a_upper, a_frame, a_origin, a_turn);
		const auto numberFits = PutOn(lower, numberAt, a_lower, a_frame, a_origin, a_turn);
		return wordFits && numberFits;
	}

	void Lines::Show(Value& a_readout, bool a_shown)
	{
		auto upper = Readout::ExtraIn(a_readout, Readout::Extra::kWord);
		auto lower = Readout::ExtraIn(a_readout, Readout::Extra::kNumber);
		Flash::Set(upper, "visible"sv, Value(a_shown));
		Flash::Set(lower, "visible"sv, Value(a_shown));
		worded.reset();
		Readout::ShowBar(a_readout, !a_shown);
		if (a_shown) {
			Readout::ShowLabel(a_readout, false);
		}
	}

	void Lines::Reset()
	{
		size = 0.0;
		word.clear();
		number.clear();
		worded.reset();
		wordAt = Readout::LabelPlace{};
		numberAt = Readout::LabelPlace{};
	}
}
