#include "Hooks.h"

namespace Hooks
{

	void All::detail::AdjustAvailableCondCobj(RE::ExtraDataList* a_item) {
		if (a_item) {
			const auto xObjectInstance = a_item->GetByType<RE::BGSObjectInstanceExtra>();
			if (xObjectInstance) {
				// Begin the loop from best omod to second-worst, adding them to the cobj. 
				// Once the attached condition omod is found, unset the remaining ones.
				auto found = false;
				for (auto i = NUM_CONDITION_LEVELS - 1; i >= 1; i--) {
					if (!found && xObjectInstance->HasMod(*g_cndObjects[i].omod)) {
						found = true;
					} else if (found) {
						// Hide condition omods lower than weapon's condition.
						g_cndObjects[i].cobj->createdItem = nullptr;
						continue;
					}
					// Show only omods that are above or equal to item's condition level
					g_cndObjects[i].cobj->createdItem = g_cndObjects[i].omod;
				}
			}

		}
	}

	RE::UI_MESSAGE_RESULTS All::ProcessMessageHk(RE::ExamineMenu* a_this, RE::UIMessage& a_message) {
		if (a_message.type.get() == RE::UI_MESSAGE_TYPE::kHide) {
			// Restore cobj created item to its corresponding omod, except the broken CND omod. 
			// Otherwise the omods won't show up when player examines the item.
			for (auto i = 1; i < NUM_CONDITION_LEVELS; i++) {
				g_cndObjects[i].cobj->createdItem = g_cndObjects[i].omod;
			}
		}
		return _ProcessMessage(a_this, a_message);
	}

	bool All::ShouldShowModSlotHk(RE::ExamineMenu* a_this, const RE::BGSKeyword* a_keyword)
	{
		if (a_this->inspectMode == false) {
			const auto& xSelectedItem = a_this->inv3DModelManager.originalExtra.get();
			if (xSelectedItem) {
				const auto xObjectInstance = xSelectedItem->GetByType<RE::BGSObjectInstanceExtra>();
				if (xObjectInstance) {
					const auto isGoodCnd = xObjectInstance->HasMod(*g_cndObjects.back().omod);  // last element is the best cnd omod.
					if (a_keyword != g_apCndKeyword && !isGoodCnd) {
						// Skip displaying regular slots if item is damaged.
						return false;
					}
					//} else if (a_keyword == g_apCndKeyword && isGoodCnd) {
					//	// Skip displaying repair slot if item is at full condition.
					//	return false;
					//}
				}
			}
		}
		return _ShouldShowModSlot(a_this, a_keyword);
	}

	void All::BuildConfirmedHk(RE::ExamineMenu* a_this, bool a_ownerIsWorkbench)
	{
		if (a_this->inspectMode == false) {
			detail::AdjustAvailableCondCobj(a_this->inv3DModelManager.originalExtra.get());
		}
		_BuildConfirmed(a_this, a_ownerIsWorkbench);
	}

	void All::UpdateModSlotListHk(RE::ExamineMenu* a_this)
	{
		if (a_this->inspectMode == false) {
			detail::AdjustAvailableCondCobj(a_this->inv3DModelManager.originalExtra.get());
		}
		_UpdateModSlotList(a_this);
	}

	void All::UpdateModChoiceListHk(RE::ExamineMenu* a_this)
	{
		_UpdateModChoiceList(a_this);
	}

	void All::PostApplyModsWeapHk(RE::TESObjectWEAP::InstanceData* a_this, const RE::TESBoundObject* a_boundObj)
	{
		_PostApplyModsWeap(a_this, a_boundObj);
	}

	bool All::WeaponFireHandlerHk(void* a_this, RE::Actor& a_actor, RE::BSFixedString const& a_bfs)
	{
		return _WeaponFireHandler(a_this, a_actor, a_bfs);
	}
}
