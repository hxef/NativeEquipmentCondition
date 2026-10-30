#include "UI/FridgeIcon.h"

#include <array>
#include <cmath>

namespace FridgeIcon
{
	namespace
	{
		// The cryo icon's frame in every damage icon clip.
		constexpr double CRYO_FRAME = 5.0;

		constexpr const char* ICON_NAME = "NEC_FridgeIcon_mc";
	}

	Value Of(Value& a_parent)
	{
		return Flash::Child(a_parent, ICON_NAME);
	}

	Value Make(Scaleform::GFx::Movie& a_movie, Value& a_parent, const char* a_class)
	{
		Value icon;
		a_movie.CreateObject(&icon, a_class);
		if (!icon.IsDisplayObject()) {
			return {};
		}
		Flash::Call(icon, "gotoAndStop", std::array{ Value(CRYO_FRAME) });
		Flash::Set(icon, "name"sv, Value(ICON_NAME));
		Flash::Set(icon, "visible"sv, Value(false));
		if (!Flash::Call(a_parent, "addChild", std::array{ icon })) {
			return {};
		}
		return icon;
	}

	std::optional<Line> LineOf(Value& a_field)
	{
		const auto line = Flash::FirstLine(a_field);
		if (!line) {
			return std::nullopt;
		}
		const auto x = Flash::Number(a_field, "x"sv);
		const auto y = Flash::Number(a_field, "y"sv);
		const auto across = Flash::Number(a_field, "scaleX"sv, 1.0);
		const auto down = Flash::Number(a_field, "scaleY"sv, 1.0);
		const Line first{ x + line->left * across, x + line->right * across, (line->baseline - line->top) * down,
			y + (line->top + line->baseline + line->descent) * down / 2.0 };
		if (!std::isfinite(first.left) || !std::isfinite(first.right) || !std::isfinite(first.capitals) || !std::isfinite(first.middle)) {
			return std::nullopt;
		}
		return first;
	}

	bool Put(Value& a_icon, const Flash::Box& a_box, Side a_side, double a_edge, double a_height, double a_middle)
	{
		const auto scale = a_height / (a_box.bottom - a_box.top);
		const auto x = a_edge - (a_side == Side::kRight ? a_box.left : a_box.right) * scale;
		const auto y = a_middle - (a_box.top + a_box.bottom) * scale / 2.0;
		if (!std::isfinite(scale) || !(scale > 0.0) || !std::isfinite(x) || !std::isfinite(y)) {
			return false;
		}
		Flash::Set(a_icon, "scaleX"sv, Value(scale));
		Flash::Set(a_icon, "scaleY"sv, Value(scale));
		Flash::Set(a_icon, "x"sv, Value(x));
		Flash::Set(a_icon, "y"sv, Value(y));
		return true;
	}
}
