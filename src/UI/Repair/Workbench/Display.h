#pragma once

#include "Core/Plugin.h"

#include <string_view>

// Keeping a weapon too worn to modify out of the mod slots, and showing which
// weapons those are. Private to this folder.
//
// Shutting the slots is the bench's own refusal: it opens them only for a
// weapon whose slot list has something in it, and otherwise plays its cancel
// sound:
//
//     public function InventoryModeToSlotsMode():* {
//        if (this._isCookingMenu || this.ModSlotList_mc.entryList.length > 0) {
//           ... open the slots ...
//        } else {
//           this.BGSCodeObj.PlaySound("UICancel");
//        }
//     }
//
// That holds for the mouse, the key and the pad, where greying the rows would
// not, since a press is handled inside the movie.
//
// A worn weapon's row is faded the way the bench fades a row of no use, by
// clearing the flags the row reads. The row draws the weapon in hand at full
// strength whatever the flags say:
//
//     textField.alpha = enabled || hasRequired || equipState > 0 ? 1 : 0.5;
//
// so a frame listener writes the faded strength onto that one name after the
// row has drawn it.
namespace Workbench
{
	// Greys out every row of one of the bench's lists by clearing the 3 flags
	// the menu reads to decide a mod can be built. The list fades and BUILD
	// greys.
	void Dim(RE::ExamineMenu* a_menu, Scaleform::GFx::Value& a_list, std::string_view a_what);

	// Hands the bench an empty list, which is all it takes to keep a worn
	// weapon out of the mod slots.
	void Clear(RE::ExamineMenu* a_menu, Scaleform::GFx::Value& a_list);

	// Fades the rows of weapons too worn to modify. The rows and the bench's
	// list of what the player carries stay in the same order.
	void FadeWorn(RE::ExamineMenu* a_menu);

	// Adds the frame listener that fades the weapon in hand to a bench movie.
	void WatchWeaponInHand(Scaleform::GFx::Movie& a_movie);
}
