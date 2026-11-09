#include "Gameplay/SpawnCondition/SpawnCondition.h"

#include "Condition/Condition.h"
#include "Core/Settings.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "Gameplay/SpawnCondition/Food.h"
#include "Gameplay/SpawnCondition/Guards.h"
#include "Gameplay/SpawnCondition/Roll.h"
#include "Gameplay/SpawnCondition/Trace.h"

namespace SpawnCondition
{
	// -------------------------------------------------------------------
	// The first look at loot lying in the world
	// -------------------------------------------------------------------

	// A faction's loose item may be a trader's stock, which the barter screen
	// rolls for the trader, see LooseItem. It never sells one an NPC owns.
	bool WaitsForTaker(const RE::TESObjectREFR& a_ref)
	{
		// The game reads the reference's extra list without checking it.
		return a_ref.extraList && a_ref.GetFactionOwner();
	}

	// The player's own list stands in for the holder, so the roll is the one
	// a pickup would give. The change is recorded first, as LooseItem does,
	// so the roll written to the world item is saved. A reference's count is
	// not read, so its trace line says x1 and several copies share 1 roll.
	bool RollOnLook(RE::TESObjectREFR& a_ref)
	{
		auto*       base = a_ref.GetObjectReference();
		auto*       extra = a_ref.extraList.get();
		const auto* player = RE::PlayerCharacter::GetSingleton();
		auto*       list = player ? player->inventoryList : nullptr;
		if (!Settings::bSpawnCondition.GetValue() || !base || !extra || !list || a_ref.IsDeleted() || WaitsForTaker(a_ref) || Loading() ||
			NotRecording()) {
			return false;
		}

		// A weapon or a piece of armor still marked for its first holder
		// rolls with every rule of Roll, as HandOver rolls it.
		if (Gear(*base)) {
			if (!Condition::IsUnrolled(extra->GetHealthPerc()) || Condition::WhyNoCondition(*base) ||
				!a_ref.AddChange(RE::CHANGE_TYPES::kObjectExtraItemData)) {
				return false;
			}
			OpenBlock(nullptr);
			Report(true, "first look", "{:<30s} [{:08X}] first look at it in the world, rolled for the player", NameOf(*base, "unnamed item"),
				base->formID);
			Roll(list, base, extra, 1, true, true);
			return true;
		}

		// Found food rolls its starting freshness as FoodFound does.
		if (!FoodSpoil::Spoils(*base) || !FoodSpoil::Unrolled(*extra) || !a_ref.AddChange(RE::CHANGE_TYPES::kObjectExtraItemData)) {
			return false;
		}
		FoodSpoil::SetStart(*extra, RollIn(*list, *base, extra, 1, "the player looked at it in the world", true));
		return true;
	}
}
