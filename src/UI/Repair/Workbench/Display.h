#pragma once

#include "Core/Plugin.h"

#include "UI/Repair/SelectedItem.h"

#include <string_view>

// Keeping an item too worn to modify out of the mod slots, and showing which
// items those are, in the list even where no mod fits them. Private to this
// folder.
//
// Shutting the slots is the bench's own refusal: it opens them only for an
// item whose slot list has something in it, and otherwise asks for a cancel
// sound by name:
//
//     public function InventoryModeToSlotsMode():* {
//        if (this._isCookingMenu || this.ModSlotBase_mc.ModSlotList_mc.entryList.length > 0) {
//           ... open the slots ...
//        } else {
//           this.BGSCodeObj.PlaySound("UICancel");
//        }
//     }
//
// That holds for the mouse, the key and the pad, where greying the rows would
// not, since a press is handled inside the movie.
//
// A worn item's row is faded the way the bench fades a row of no use, by
// clearing the flags the row reads. The row draws an equipped item at full
// strength whatever the flags say, the weapon in hand and each piece of armor
// worn alike:
//
//     textField.alpha = enabled || hasRequired || equipState > 0 ? 1 : 0.5;
//
// so a frame listener writes the faded strength onto those names after the
// rows have drawn them.
//
// The bench leaves an item out of its list when none of its slots takes a mod
// here, a hazmat suit for example, by writing 0 into the row's filterFlag,
// which the list's filter never lets through:
//
//     return entry != null && (!entry.hasOwnProperty("filterFlag")
//                              || (entry.filterFlag & this.iItemFilter) != 0);
//
// A worn one of the bench's own kind gets the filter's flag instead, so it can
// still be repaired. Its row stays greyed whatever its condition, since no mod
// will ever fit it, and a message in the corner says so once a repair brings it
// to full and whenever its slots are asked for.
namespace Workbench
{
	// Greys out every row of one of the bench's lists by clearing the 3 flags
	// the menu reads to decide a mod can be built. The list fades and BUILD
	// greys.
	void Dim(RE::ExamineMenu* a_menu, Scaleform::GFx::Value& a_list, std::string_view a_what);

	// Hands the bench an empty list, which is all it takes to keep a worn item
	// out of the mod slots.
	void Clear(RE::ExamineMenu* a_menu, Scaleform::GFx::Value& a_list);

	// Marks the rows the bench has built and not yet drawn. An item too worn to
	// modify is faded. A worn item the bench left out, since no mod fits it
	// here, is listed after all, greyed, and every stack of it stays listed
	// until the bench closes, repaired or not, so a repair never takes the row
	// out from under the highlight. The rows and the bench's list of what the
	// player carries stay in the same order.
	void MarkWorn(RE::ExamineMenu* a_menu);

	// Whether no mod fits an item here, which is whether MarkWorn listed it
	// since the bench opened.
	[[nodiscard]] bool NoModFits(const SelectedItem::Item& a_item);

	// Starts a bench with nothing listed that way.
	void ForgetListed();

	// Adds the frame listener that fades the equipped items and keeps the
	// button's word, see Relabel in Label.h, to a bench movie.
	void WatchEquipped(Scaleform::GFx::Movie& a_movie);
}
