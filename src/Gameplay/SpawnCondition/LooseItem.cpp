#include "Gameplay/SpawnCondition/Handover.h"

#include "Condition/Condition.h"
#include "Gameplay/SpawnCondition/Roll.h"
#include "Gameplay/SpawnCondition/Trace.h"

namespace SpawnCondition
{
	// -------------------------------------------------------------------
	// A trader's loose item
	// -------------------------------------------------------------------

	// Compares the 2 pointers only. The global names the barter copy while the
	// barter menu is open, and other threads read it too, so it is read fresh
	// on every call and never read through.
	bool BarterCopy(const RE::BGSInventoryList* a_list)
	{
		const auto* barter = RE::TESDataHandler::GetBarterContainer();
		if (!barter || !a_list) {
			return false;
		}
		const auto owner = a_list->owner.get();
		return owner.get() == barter;
	}

	// The barter screen copies such an item with the world item's health and
	// a tag naming it, as 1 stack of its whole count, or 1 copy at a time
	// when the copy still needs its mods or its form asks for 1 at a time.
	// The first copy rolls, and the world item takes the same roll, so every
	// later copy, a rebuild of the screen and the item bought all read it.
	// The trade that buys it puts the mark back, see NoteBought. A piece
	// that gives nothing is sold new. Lock order: the barter copy's
	// inventory, held by the caller, then the change record, then the world
	// item's extra data.
	bool LooseItem(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack)
	{
		if (!a_object || !a_stack || !a_stack->extra || !Gear(*a_object)) {
			return false;
		}
		auto& copy = *a_stack->extra;
		if (!Condition::IsUnrolled(copy.GetHealthPerc()) || Condition::WhyNoCondition(*a_object)) {
			return false;
		}
		const auto* tag = copy.GetByType<RE::ExtraOriginalReference>();
		const auto  world = tag ? tag->originalReference.get() : nullptr;
		auto*       worldExtra = world ? world->extraList.get() : nullptr;
		if (!worldExtra) {
			return false;
		}

		const auto name = NameOf(*a_object, "unnamed item");
		const auto id = a_object->formID;
		const auto count = a_stack->count;
		OpenBlock(nullptr);

		const auto held = worldExtra->GetHealthPerc();
		if (held >= 0.0F && !Condition::IsUnrolled(held)) {
			copy.SetHealthPerc(held);
			Report(true, "a trader's loose item, already rolled", "{:<30s} [{:08X}] x{:<3d} starts at {:.4f}, a trader's loose item, already rolled",
				name, id, count, held);
			return true;
		}
		if (!Condition::IsUnrolled(held)) {
			return false;
		}

		// Recorded first, so the roll written to the world item is saved. A
		// legendary arrives new, as a showpiece, since the copy is a chest.
		world->AddChange(RE::CHANGE_TYPES::kObjectExtraItemData);
		if (SoldNew(*a_object)) {
			copy.SetHealthPerc(Condition::MAX_HEALTH);
			worldExtra->SetHealthPerc(Condition::MAX_HEALTH);
			Report(true, "a trader's loose item, sold new", "{:<30s} [{:08X}] x{:<3d} starts at {:.4f}, a trader's loose item, sold new as it gives nothing",
				name, id, count, Condition::MAX_HEALTH);
			return true;
		}
		Roll(a_list, a_object, a_stack);
		const auto rolled = copy.GetHealthPerc();
		if (rolled >= 0.0F && !Condition::IsUnrolled(rolled)) {
			worldExtra->SetHealthPerc(rolled);
		}
		Report(true, "a trader's loose item", "{:<30s} [{:08X}] x{:<3d} starts at {:.4f}, a trader's loose item, and so does the one in the world",
			name, id, count, rolled);
		return true;
	}
}
