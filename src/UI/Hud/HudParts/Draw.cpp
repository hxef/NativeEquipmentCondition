#include "UI/Hud/HudParts/HudParts.h"

#include "UI/Flash.h"

#include <algorithm>
#include <array>
#include <format>

namespace HudParts
{
	void SetStyledText(Value& a_field, std::string_view a_text, std::int32_t a_size)
	{
		const auto html = std::format(R"(<font face="{:s}" size="{}" color="#FFFFFF">{:s}</font>)", FONT_NAME, a_size,
			Flash::HtmlEscaped(a_text));
		a_field.SetMember("htmlText"sv, Value(std::string_view{ html }));
	}

	void SetTranslatedText(Value& a_field, const char* a_key, std::int32_t a_size)
	{
		a_field.SetMember("text"sv, Value(a_key));
		SetStyledText(a_field, Flash::String(a_field, "text"sv), a_size);
	}

	void PutBox(Value& a_box, double a_x, double a_y, double a_width, double a_height)
	{
		a_box.SetMember("x"sv, Value(a_x));
		a_box.SetMember("y"sv, Value(a_y));
		a_box.SetMember("scaleX"sv, Value(std::max(a_width, 0.0)));
		a_box.SetMember("scaleY"sv, Value(std::max(a_height, 0.0)));
	}

	bool AddBox(Scaleform::GFx::Movie& a_movie, Value& a_parent, const char* a_name)
	{
		Value box;
		a_movie.CreateObject(&box, "flash.display.Sprite");
		if (!box.IsDisplayObject()) {
			return false;
		}

		Value graphics;
		if (!box.GetMember("graphics"sv, &graphics) ||
			!graphics.Invoke("beginFill", std::array{ Value(0xFFFFFF), Value(1.0) }) ||
			!graphics.Invoke("drawRect", std::array{ Value(0.0), Value(0.0), Value(1.0), Value(1.0) }) ||
			!graphics.Invoke("endFill")) {
			return false;
		}

		box.SetMember("name"sv, Value(a_name));
		return a_parent.Invoke("addChild", std::array{ box });
	}

	bool AddField(Scaleform::GFx::Movie& a_movie, Value& a_parent, const char* a_name)
	{
		Value field;
		a_movie.CreateObject(&field, "flash.text.TextField");
		if (!field.IsDisplayObject()) {
			return false;
		}

		field.SetMember("name"sv, Value(a_name));

		// The HUD font lives in the engine's font library, which is what
		// embedFonts asks for.
		field.SetMember("embedFonts"sv, Value(true));
		field.SetMember("selectable"sv, Value(false));
		field.SetMember("mouseEnabled"sv, Value(false));
		field.SetMember("autoSize"sv, Value("left"));
		return a_parent.Invoke("addChild", std::array{ field });
	}
}
