#include "UI/Repair/Restore.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Gameplay/ArmorRating.h"

namespace Restore
{
	void Write(RE::PlayerCharacter& a_player, RE::TESBoundObject& a_object,
		RE::BGSInventoryItem::StackDataCompareFunctor& a_find, std::uint32_t a_level)
	{
		RE::BGSInventoryItem::SetHealthFunctor set{ Condition::FromPercent(a_level) };
		Equipped::WriteStack(a_player, a_object, a_find, set);

		if (Condition::KindOf(a_object) == Condition::Kind::kArmor) {
			ArmorRating::Refresh(a_player);
		}
	}
}
