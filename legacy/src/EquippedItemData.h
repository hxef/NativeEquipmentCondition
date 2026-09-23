#pragma once

// LEGACY, not built, and it will not compile as it stands. It refers to the ESP
// globals g_cndObjects and NUM_CONDITION_LEVELS, which no longer exist.
//
// A superseded way of finding, splitting and initialising the equipped stack.
// It walked the inventory by hand, split a stack of more than one item itself by
// building every extra data copy field by field, and charged a hardcoded two
// percent per event whatever the weapon was.
//
// The engine does the same work correctly through
// TESObjectREFR::FindAndWriteStackDataForInventoryItem, which takes the
// inventory lock, picks the stack, splits it, moves the equipped flags onto the
// copy and merges anything identical afterwards. Degradation::WearEquipped uses
// that instead, and the rate now comes from what the weapon deals and what it is
// built from rather than from a constant.
//
// Nothing ever called any of this. `inv` was never assigned, so FindEquipped
// would have read through an uninitialised pointer had it been reached.
//
// WorkbenchMenuOpened at the bottom was one of the three Papyrus natives the old
// DLL exported, declared in legacy/papyrus/HxfItemDegradation/HxfItemDegradation.psc.
// The current DLL exports no Papyrus natives.

#include "Common.h"

namespace Degradation
{
	// Note: this class write-locks actor's inventory during its lifetime.
	class EquippedItemData
	{
	public:
		// Returns a vector of mods attached to the BGSObjectInstanceExtra parameter.
		inline static std::vector<RE::BGSMod::Attachment::Mod*> GetModsFromObjectExtra(const RE::BGSObjectInstanceExtra* xObj)
		{
			std::vector<RE::BGSMod::Attachment::Mod*> attachedMods;
			const auto indices = xObj->GetIndexData();
			for (const auto& idx : indices) {
				attachedMods.push_back(RE::TESForm::GetFormByID<RE::BGSMod::Attachment::Mod>(idx.objectID));
			}
			return attachedMods;
		}

		inline bool FindEquipped()
		{
			for (const auto& item : inv->data) {
				if (!IsSupportedType(item)) {
					continue;
				}

				for (auto stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
					auto& xStack = stack->extra;
					if (!stack->IsEquipped() || !xStack) {
						continue;
					}

					// equipped items that have no ExtraInstanceData or BGSObjectInstanceExtra should be skipped,
					// given that all items that support condition mods are expected to have an instance data(because they should have one condition OMOD)
					const auto xInstanceData = xStack->GetByType<RE::ExtraInstanceData>();
					if (!xInstanceData) {
						continue;
					}
					const auto xObjectInstance = xStack->GetByType<RE::BGSObjectInstanceExtra>();
					if (!xObjectInstance) {
						continue;
					}

					const auto instanceData = xInstanceData->data.get();
					if (!instanceData) {
						continue;
					}

					if (!GetExpectedData(instanceData)) {
						continue;
					}

					if (stack->count > 1 && !xStack->HasType(RE::EXTRA_DATA_TYPE::kUniqueID)) {
						// create the split stack
						auto split = RE::BSTSmartPointer(RE::calloc<RE::BGSInventoryItem::Stack>(1));
						REX::EMPLACE_VTABLE(split.get());

						// copy only the flags not related to slot index
						using Flag = RE::BGSInventoryItem::Stack::Flag;
						split->flags = stack->flags;
						split->flags.reset(Flag::kSlotMask);

						// create split stack's extra data list
						auto& xSplit = split->extra;
						xSplit = RE::BSTSmartPointer(new RE::ExtraDataList);

						// create a copy of stack's BGSObjectInstanceExtra and attach it to the split stack
						auto xSplitObjInstance = new RE::BGSObjectInstanceExtra();
						// Walk the index data rather than GetModsFromObjectExtra() so that each
						// copied mod keeps its attach index and rank, which a plain mod list drops.
						for (const auto& idx : xObjectInstance->GetIndexData()) {
							auto* mod = RE::TESForm::GetFormByID<RE::BGSMod::Attachment::Mod>(idx.objectID);
							if (mod) {
								xSplitObjInstance->AddMod(*mod, idx.index, idx.rank, false);
							}
						}
						xSplitObjInstance->flags = xObjectInstance->flags;
						xSplit->AddExtra(xSplitObjInstance);

						// create ExtraInstanceData from the BGSObjectInstanceExtra created above and attach it to the split stack
						RE::BSTSmartPointer<RE::TBO_InstanceData> xSplitTBO_InstanceData;
						xSplitObjInstance->CreateBaseInstanceData(*item.object, xSplitTBO_InstanceData);
						auto xSplitInstanceData = new RE::ExtraInstanceData(item.object, xSplitTBO_InstanceData);
						xSplit->AddExtra(xSplitInstanceData);

						// create a copy of stack's ExtraTextDisplayData and attach it to the split stack
						auto xStackTextDisplayData = xStack->GetByType<RE::ExtraTextDisplayData>();
						if (xStackTextDisplayData) {
							auto xSplitTextDisplayData = new RE::ExtraTextDisplayData(*xStackTextDisplayData);
							REX::EMPLACE_VTABLE(xSplitTextDisplayData);
							xSplitTextDisplayData->next = nullptr;
							xSplit->AddExtra(xSplitTextDisplayData);
						}

						// create a copy of stack's ExtraHealth and attach it to the split stack
						auto xStackHealth = xStack->GetByType<RE::ExtraHealth>();
						if (xStackHealth) {
							auto xSplitHealth = new RE::ExtraHealth(*xStackHealth);
							REX::EMPLACE_VTABLE(xSplitHealth);
							xSplitHealth->next = nullptr;
							xSplit->AddExtra(xSplitHealth);
						}

						// adjust stack counts
						split->count = stack->count - 1;
						stack->count = 1;

						// add the created split stack to the inventory item's stack
						auto it = stack;
						while (true) {
							if (!it->nextStack.get()) {
								it->nextStack = split;
								break;
							}
							it = it->nextStack.get();
						}
					}

					// init base members
					this->instanceDataExtra = xInstanceData;
					this->tboInstanceData = instanceData;
					this->objectInstanceExtra = xObjectInstance;
					this->health = &InitConditionIfNeeded(stack);

					return true;
				}
			}
			return false;
		}

		// Returns the health of the item
		inline float GetHealth()
		{
			return *health;
		}

	private:
		RE::ExtraInstanceData*      instanceDataExtra;
		RE::TBO_InstanceData*       tboInstanceData;
		RE::BGSObjectInstanceExtra* objectInstanceExtra;
		RE::BGSInventoryList*       inv;
		// pointer to health value of the health extradata
		float* health;

		virtual bool IsSupportedType(const RE::BGSInventoryItem&) = 0;
		virtual bool GetExpectedData(RE::TBO_InstanceData* const) = 0;
		// Returns how much health should be decreased for each degradation event.
		virtual float GetDegradationRate() = 0;

		// Initializes health extradata and condition omod of the stack item if not present, to full condition.
		// Returns the reference to the health value of the health extradata.
		inline float& InitConditionIfNeeded(RE::BGSInventoryItem::Stack* const stack)
		{
			auto xHealth = stack->extra->GetByType<RE::ExtraHealth>();
			if (!xHealth) {
				// initialize health extradata for this item to max health
				// ExtraHealth's constructor sets the extra-data type and emplaces the
				// vtable, so unlike the extras above this one needs no EMPLACE_VTABLE.
				xHealth = new RE::ExtraHealth(MAX_HEALTH);
				stack->extra->AddExtra(xHealth);

				// attach full condition mod
				// AttachMod(*g_cndObjects[NUM_CONDITION_LEVELS - 1].omod);
			}
			return xHealth->health;
		}
	};

	class EquippedWeapon : public EquippedItemData
	{
		using EquippedItemData::EquippedItemData;

	private:
		RE::TESObjectWEAP::InstanceData* eqWeaponInstanceData;

		bool IsSupportedType(const RE::BGSInventoryItem& item) override
		{
			if (item.object->IsWeapon()) {
				return true;
			}
			return false;
		}

		bool GetExpectedData(RE::TBO_InstanceData* const instanceData) override
		{
			const auto weaponInstance = RE::fallout_cast<RE::TESObjectWEAP::InstanceData*>(const_cast<RE::TBO_InstanceData*>(instanceData));
			if (!weaponInstance) {
				return false;
			}

			if (weaponInstance->type == RE::WEAPON_TYPE::kGrenade || weaponInstance->type == RE::WEAPON_TYPE::kMine) {
				// throwables not supported.
				return false;
			}

			this->eqWeaponInstanceData = weaponInstance;
			return true;
		}

		float GetDegradationRate() override
		{
			return 0.02f;
		}
	};

	// One of the three Papyrus natives the old DLL exported. It built a map of
	// every OMOD in the load order to the components its recipe asks for, then
	// noted which inventory item the workbench was hovering. Nothing was ever
	// done with either.
	//
	// inline: defined in a header included by several translation units.
	inline void WorkbenchMenuOpened(std::monostate, bool bOpened)
	{
		// A map of OMOD pointers to vector of (component form, count) pairs that make up the OMOD, sorted by value.
		static auto cmp = [](const auto& a, const auto& b) -> bool {
			auto* value_a = RE::fallout_cast<RE::TESValueForm*>(std::get<0>(a));
			auto* value_b = RE::fallout_cast<RE::TESValueForm*>(std::get<0>(b));
			if (!value_a) {
				return false;
			} else if (!value_b) {
				return true;
			}
			return value_a->value < value_b->value;
		};
		static std::unordered_map<RE::BGSMod::Attachment::Mod*, std::set<std::pair<RE::TESForm*, std::uint32_t>, decltype(cmp)>> omodRequirements;
		static RE::TESBoundObject* moddedInventoryItem;

		// Clean up static vars if workbench menu was closed
		if (!bOpened) {
			omodRequirements.clear();
			moddedInventoryItem = nullptr;
			return;
		}

		const auto ui = RE::UI::GetSingleton();
		auto examineMenu = ui->GetMenu<RE::ExamineMenu>();
		if (!examineMenu || examineMenu->inspectMode) {
			return;
		}

		// build the omod requirements map if empty
		if (omodRequirements.empty()) {
			for (auto& cobjForm : g_dataHandler->GetFormArray<RE::BGSConstructibleObject>()) {
				auto* createdMod = RE::fallout_cast<RE::BGSMod::Attachment::Mod*>(cobjForm->createdItem);
				if (!createdMod) {
					continue;
				}
				auto* requiredItems = cobjForm->requiredItems;
				if (!requiredItems) {
					continue;
				}

				for (auto* req = requiredItems->begin(); req != requiredItems->end(); req++) {
					omodRequirements[createdMod].insert(std::make_pair(req->first, req->second.i));
				}
			}
		}

		// Update the condition cobjs requirements when player hovers over another inventory item
		if (moddedInventoryItem != examineMenu->moddedInventoryItem.object) {
			moddedInventoryItem = examineMenu->moddedInventoryItem.object;
		}
	}
}
