#include "UI/Hud/QuickContainer/Draw.h"

#include "UI/Flash.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>

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

	void Draw(Value& a_meter, std::int32_t a_percent, bool a_selected, bool a_fromRight)
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
			Rect(graphics, a_fromRight ? METER_WIDTH - 2.0 - bar : 2.0, top + 2.0, bar, METER_HEIGHT - 4.0);
		}

		graphics.Invoke("endFill");
	}
	// -------------------------------------------------------------------
	// Writing one number
	// -------------------------------------------------------------------

	std::optional<Letters> LettersOf(const Value& a_name)
	{
		const auto format = Flash::Member(a_name, "defaultTextFormat"sv);
		const auto font = Flash::Member(format, "font"sv);
		const auto size = Flash::Number(format, "size"sv);
		if (!font.IsString() || !*font.GetString() || !std::isfinite(size) || !(size > 0.0)) {
			return std::nullopt;
		}
		return Letters{ font.GetString(), size };
	}

	std::optional<double> Write(Value& a_field, std::string_view a_text, const Letters& a_letters, bool a_selected)
	{
		// A font tag takes a whole size only, so the field is scaled to the
		// size asked for, see HudParts::SetStyledText.
		const auto written = std::max(1L, std::lround(a_letters.size));
		const auto scale = NUMBER_SHARE * a_letters.size / static_cast<double>(written);
		const auto html = std::format(R"(<font face="{:s}" size="{:d}" color="#{:06X}">{:s}</font>)", Flash::HtmlEscaped(a_letters.font), written,
			a_selected ? BLACK : WHITE, Flash::HtmlEscaped(a_text));
		if (!Flash::Set(a_field, "htmlText"sv, Value(std::string_view{ html }))) {
			return std::nullopt;
		}
		Flash::Set(a_field, "scaleX"sv, Value(scale));
		Flash::Set(a_field, "scaleY"sv, Value(scale));
		const auto line = Flash::FirstLine(a_field);
		return line ? std::optional{ (line->right - line->left) * scale } : std::nullopt;
	}
}
