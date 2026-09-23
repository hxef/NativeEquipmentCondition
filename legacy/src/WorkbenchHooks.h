#pragma once

// LEGACY, not built. The workbench repair UI, which drove the condition OMODs
// and repair recipes that shipped in HxfItemDegradation.esp. The mod is a DLL
// only now, so there are no such forms to show and nothing here can run.
//
// The fire and reload hooks that used to sit alongside these have moved to
// src/Gameplay/WeaponEvents, since neither needed the ESP.

#include "CallPatch.h"
#include "Common.h"

namespace Hooks
{

	class All
	{
	public:
		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RE::ExamineMenu::VTABLE[0] };

			_BuildConfirmed = target.write_vfunc(0x17, reinterpret_cast<std::uintptr_t>(BuildConfirmedHk));
			_UpdateModSlotList = target.write_vfunc(0x3E, reinterpret_cast<std::uintptr_t>(UpdateModSlotListHk));
			_UpdateModChoiceList = target.write_vfunc(0x3F, reinterpret_cast<std::uintptr_t>(UpdateModChoiceListHk));
			_ShouldShowModSlot = target.write_vfunc(0x45, reinterpret_cast<std::uintptr_t>(ShouldShowModSlotHk));
			_ProcessMessage = target.write_vfunc(0x3, reinterpret_cast<std::uintptr_t>(ProcessMessageHk));
			//_CreateModdedInventoryItem = target.write_vfunc(0x2A, reinterpret_cast<std::uintptr_t>(CreateModdedInventoryItemHk));
			//_PopulateInventoryItemObj = target.write_vfunc(0x38, reinterpret_cast<std::uintptr_t>(PopulateInventoryItemObjHk));
			//_GetInitialObjectInstanceExtra = target.write_vfunc(0x25, reinterpret_cast<std::uintptr_t>(GetInitialObjectInstanceExtraHk));

			REL::Relocation<std::uintptr_t> target3{ RE::TESObjectWEAP::InstanceData::VTABLE[0] };
			_PostApplyModsWeap = target3.write_vfunc(0x12, reinterpret_cast<std::uintptr_t>(PostApplyModsWeapHk));

		}

	private:

		/*Hook when player confirms building something at the workbench.*/
		static void BuildConfirmedHk(RE::ExamineMenu* a_this, bool a_ownerIsWorkbench);
		inline static REL::Relocation<decltype(&BuildConfirmedHk)> _BuildConfirmed;

		/*Hook when the mod slot list is updated at the workbench.*/
		static void UpdateModSlotListHk(RE::ExamineMenu* a_this);
		inline static REL::Relocation<decltype(&UpdateModSlotListHk)> _UpdateModSlotList;

		/*Hook when the mod choice list is updated at the workbench.*/
		static void UpdateModChoiceListHk(RE::ExamineMenu* a_this);
		inline static REL::Relocation<decltype(&UpdateModChoiceListHk)> _UpdateModChoiceList;

		/*Hook to function that determines whether a mod slot would appear in the workbench menu.
		Used to prevent workbench modifications on damaged items.*/
		static bool ShouldShowModSlotHk(RE::ExamineMenu* a_this, const RE::BGSKeyword* a_keyword);
		inline static REL::Relocation<decltype(&ShouldShowModSlotHk)> _ShouldShowModSlot;

		static RE::UI_MESSAGE_RESULTS ProcessMessageHk(RE::ExamineMenu* a_this, RE::UIMessage& a_message);
		inline static REL::Relocation<decltype(&ProcessMessageHk)> _ProcessMessage;

		static void PostApplyModsWeapHk(RE::TESObjectWEAP::InstanceData* a_this, const RE::TESBoundObject* a_boundObj);
		inline static REL::Relocation<decltype(&PostApplyModsWeapHk)> _PostApplyModsWeap;

		// helpers
		class detail
		{
			friend All;
			//Hides the condition omods from the crafting menu that are below the item's current
			//condition level, by removing the corresponding cobj created item.
			static void AdjustAvailableCondCobj(RE::ExtraDataList* a_item);
		};
	};
}
