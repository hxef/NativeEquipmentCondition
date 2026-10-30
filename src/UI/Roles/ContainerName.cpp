#include "UI/Roles/ContainerName.h"

#include "UI/Flash.h"

namespace Roles::ContainerName
{
	namespace
	{
		// Vanilla's place, read off HUDMenu.swf.
		constexpr const char* NAME_PATH = "_root.CenterGroup_mc.QuickContainerWidget_mc.ListHeaderAndBracket_mc.ContainerName_mc";
		constexpr const char* FIELD_NAME = "textField_tf";
	}

	std::optional<Value> Find(Scaleform::GFx::Movie& a_movie)
	{
		Value clip;
		if (!a_movie.GetVariable(&clip, NAME_PATH) || !clip.IsDisplayObject() || !Field(clip).IsDisplayObject()) {
			return std::nullopt;
		}
		return clip;
	}

	Value Field(const Value& a_clip)
	{
		auto field = Flash::Member(a_clip, FIELD_NAME);
		return field.IsDisplayObject() ? field : Value{};
	}
}
