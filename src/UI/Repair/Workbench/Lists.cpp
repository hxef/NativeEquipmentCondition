#include "UI/Repair/Workbench/Lists.h"

#include "Core/CallPatch/CallPatch.h"
#include "UI/Repair/Workbench/Bench.h"
#include "UI/Repair/Workbench/Display.h"
#include "UI/Repair/Workbench/Workbench.h"

#include <cstddef>
#include <cstdint>

namespace Workbench
{
	namespace
	{
		// What the game does before this, which every hook calls straight away
		// while the bench's places are not all NEC's, see Repairs.
		REL::Relocation<void (*)(RE::ExamineMenu*, std::int32_t)> _UpdateItemList;
		REL::Relocation<void (*)(RE::ExamineMenu*)>               _UpdateModSlotList;
		REL::Relocation<void (*)(RE::ExamineMenu*)>               _UpdateModChoiceList;

		// Where the bench's rebuild of its item list hands the new rows to
		// Flash, which draws them and puts the highlight back on the item it
		// was on, or on the nearest row shown.
		constexpr CallPatch::CallSite REFRESH_SITE{ 2223054, 0x245, "item list refresh" };

		// The bench while it rebuilds its item list. The bench is a menu, so
		// only its own thread comes here.
		RE::ExamineMenu* g_rebuilding{ nullptr };

		CallPatch::Link<bool(Scaleform::GFx::Value::ObjectInterface*, void*, Scaleform::GFx::Value*, const char*,
			const Scaleform::GFx::Value*, std::size_t, bool)>
			g_refreshLink;

		// The bench's own inventory, rebuilt, see RefreshItemListHk.
		void UpdateItemListHk(RE::ExamineMenu* a_menu, std::int32_t a_selected)
		{
			if (!Repairs()) {
				_UpdateItemList(a_menu, a_selected);
				return;
			}
			g_rebuilding = a_menu;
			_UpdateItemList(a_menu, a_selected);
			g_rebuilding = nullptr;
		}

		// The new rows on their way to Flash. Every worn item the bench left
		// out is listed after all and every item too worn to modify is faded
		// first, so the list is drawn once and the highlight comes back to a
		// listed row as to any other. Listed any later, the row would still be
		// hidden as the highlight came back, which moves it off a hazmat suit
		// just repaired. The power armor station runs the same rebuild without
		// the hook above, and its rows go as they are.
		bool RefreshItemListHk(Scaleform::GFx::Value::ObjectInterface* a_interface, void* a_data, Scaleform::GFx::Value* a_result,
			const char* a_name, const Scaleform::GFx::Value* a_args, std::size_t a_count, bool a_isDisplayObject)
		{
			if (g_rebuilding && Repairs()) {
				MarkWorn(g_rebuilding);
			}
			return g_refreshLink(a_interface, a_data, a_result, a_name, a_args, a_count, a_isDisplayObject);
		}

		// An item too worn to modify is given no slots at all.
		void UpdateModSlotListHk(RE::ExamineMenu* a_menu)
		{
			_UpdateModSlotList(a_menu);
			if (Repairs() && a_menu && Selected(a_menu).TooWorn()) {
				Clear(a_menu, a_menu->modSlotList);
			}
		}

		// The mods behind a slot, greyed. The bench fills the list on its own
		// account too, so it is worth leaving shut.
		void UpdateModChoiceListHk(RE::ExamineMenu* a_menu)
		{
			_UpdateModChoiceList(a_menu);
			if (Repairs() && a_menu && Selected(a_menu).TooWorn()) {
				Dim(a_menu, a_menu->modChoiceList, "mod"sv);
			}
		}
	}

	void InstallLists()
	{
		REL::Relocation<std::uintptr_t> menu{ RE::ExamineMenu::VTABLE[0] };

		_UpdateItemList = CallPatch::PatchSlot(menu, 0x2F, UpdateItemListHk, "bench item list").value_or(0);
		_UpdateModSlotList = CallPatch::PatchSlot(menu, 0x3E, UpdateModSlotListHk, "bench mod slots").value_or(0);
		_UpdateModChoiceList = CallPatch::PatchSlot(menu, 0x3F, UpdateModChoiceListHk, "bench mod choices").value_or(0);

		CallPatch::PatchCall(REFRESH_SITE, Scaleform::ID::GFx::Value::Invoke.address(),
			reinterpret_cast<std::uintptr_t>(&RefreshItemListHk), g_refreshLink);
	}
}
