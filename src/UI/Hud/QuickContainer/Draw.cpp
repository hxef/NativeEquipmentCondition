#include "UI/Hud/QuickContainer/Draw.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace QuickContainer
{
	namespace
	{
		using Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// Drawing one meter
		// -------------------------------------------------------------------

		// The row's own text colours: white, and black on the highlighted row,
		// whose bright bar would hide white.
		constexpr std::uint32_t WHITE = 0xFFFFFF;
		constexpr std::uint32_t BLACK = 0x000000;

		void Rect(Value& a_graphics, double a_x, double a_y, double a_width, double a_height)
		{
			a_graphics.Invoke("drawRect", std::array{ Value(a_x), Value(a_y), Value(a_width), Value(a_height) });
		}
	}

	void Draw(Value& a_meter, std::int32_t a_percent, bool a_selected)
	{
		Value graphics;
		if (!a_meter.GetMember("graphics"sv, &graphics) || !graphics.IsObject()) {
			return;
		}

		const auto top = -METER_HEIGHT / 2.0;
		const auto bar = (METER_WIDTH - 4.0) * std::clamp(a_percent, 0, 100) / 100.0;

		graphics.Invoke("clear");
		graphics.Invoke("beginFill", std::array{ Value(a_selected ? BLACK : WHITE), Value(1.0) });

		// 4 strips that never overlap, since overlapping shapes in one fill
		// cut holes in each other.
		Rect(graphics, 0.0, top, METER_WIDTH, 1.0);
		Rect(graphics, 0.0, top + METER_HEIGHT - 1.0, METER_WIDTH, 1.0);
		Rect(graphics, 0.0, top + 1.0, 1.0, METER_HEIGHT - 2.0);
		Rect(graphics, METER_WIDTH - 1.0, top + 1.0, 1.0, METER_HEIGHT - 2.0);

		// The bar, a pixel clear of the frame all round.
		if (bar > 0.0) {
			Rect(graphics, 2.0, top + 2.0, bar, METER_HEIGHT - 4.0);
		}

		graphics.Invoke("endFill");
	}
}
