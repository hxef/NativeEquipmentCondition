#include "UI/Hud/HudCondition/Place.h"

#include "Core/Settings.h"
#include "UI/Flash.h"

#include <cmath>
#include <numbers>

namespace HudCondition
{
	using Scaleform::GFx::Value;

	Shift ShiftNow()
	{
		const Shift shift{ Settings::fHudBarX.GetValue(), Settings::fHudBarY.GetValue() };

		// A nan typed into the ini never equals itself, so the bar would be
		// placed again every frame. It counts as 0 and 0.
		return std::isfinite(shift.x) && std::isfinite(shift.y) ? shift : Shift{};
	}

	Transform TransformOf(const Value& a_clip)
	{
		return Transform{ Flash::Number(a_clip, "x"sv), Flash::Number(a_clip, "y"sv),
			Flash::Number(a_clip, "scaleX"sv), Flash::Number(a_clip, "scaleY"sv), Flash::Number(a_clip, "rotation"sv) };
	}

	void Place(Value& a_readout, const Transform& a_counter, const Roles::Hud::Line& a_spot, const Shift& a_shift)
	{
		const auto turn = a_counter.rotation * std::numbers::pi / 180.0;
		const auto across = a_spot.x * a_counter.scaleX;
		const auto down = a_spot.y * a_counter.scaleY;
		a_readout.SetMember("x"sv, Value(a_counter.x + (across * std::cos(turn)) - (down * std::sin(turn)) + a_shift.x));
		a_readout.SetMember("y"sv, Value(a_counter.y + (across * std::sin(turn)) + (down * std::cos(turn)) - a_shift.y));
		a_readout.SetMember("scaleX"sv, Value(a_counter.scaleX));
		a_readout.SetMember("scaleY"sv, Value(a_counter.scaleY));
		a_readout.SetMember("rotation"sv, Value(a_counter.rotation));
	}
}
