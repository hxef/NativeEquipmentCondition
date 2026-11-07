#include "UI/Roles/PerkCard.h"

#include "UI/Flash.h"

namespace Roles::PerkCard
{
	namespace
	{
		// Vanilla's place, read off LevelUpMenu.swf, the same on OG and AE.
		constexpr const char* DESCRIPTION_PATH = "_root.Menu_mc.PerkInfo_mc.DescriptionText_tf";
	}

	std::optional<Value> Description(Scaleform::GFx::Movie& a_movie)
	{
		Value field;
		if (!a_movie.GetVariable(&field, DESCRIPTION_PATH) || !field.IsDisplayObject() ||
			!Flash::Member(field, "text"sv).IsString() || !Flash::Member(field, "autoSize"sv).IsString()) {
			Noted(a_movie, "perk card description"sv, "a long perk description shows its first 2 lines"sv);
			return std::nullopt;
		}
		Found(a_movie, "perk card description"sv, "at vanilla's place"sv);
		return field;
	}
}
