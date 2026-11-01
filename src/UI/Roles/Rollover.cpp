#include "UI/Roles/Rollover.h"

#include "UI/Flash.h"

namespace Roles::Rollover
{
	namespace
	{
		// Vanilla's place, read off HUDMenu.swf.
		constexpr const char* PROMPT_PATH = "_root.CenterGroup_mc.RolloverWidget_mc";
		constexpr const char* NAME_FIELD = "RolloverName_tf";
		constexpr const char* HINTS_NAME = "ButtonHintBar_mc";
	}

	std::optional<Value> Find(Scaleform::GFx::Movie& a_movie)
	{
		Value prompt;
		if (!a_movie.GetVariable(&prompt, PROMPT_PATH) || !prompt.IsDisplayObject() || !Name(prompt).IsDisplayObject() ||
			!Hints(prompt).IsDisplayObject()) {
			return std::nullopt;
		}
		return prompt;
	}

	Value Name(const Value& a_prompt)
	{
		auto field = Flash::Member(a_prompt, NAME_FIELD);
		return field.IsDisplayObject() ? field : Value{};
	}

	Value Hints(const Value& a_prompt)
	{
		auto hints = Flash::Member(a_prompt, HINTS_NAME);
		return hints.IsDisplayObject() ? hints : Value{};
	}
}
