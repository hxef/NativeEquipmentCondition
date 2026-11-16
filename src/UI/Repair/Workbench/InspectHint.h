#pragma once

#include "Core/Plugin.h"

#include "UI/Roles/Bars.h"

#include <optional>
#include <string_view>

// NEC's REPAIR hint on the bar of the Pip-Boy's inspect screen, see Inspect.h.
// The menu's movie keeps no member NEC adds, so the hint is found again by
// its word on every bar the menu has, REPAIR or MEND, which no inspect list
// holds of its own. A hint found is let go of before the call that found it
// returns. Private to this folder.
namespace Workbench::Inspect
{
	using Value = Scaleform::GFx::Value;

	// NEC's hint on any bar of a_menu, or undefined while it has none.
	[[nodiscard]] Value FindHint(RE::ExamineMenu& a_menu);

	// Whether a_hint is on a bar, shown and enabled.
	[[nodiscard]] bool Offered(const Value& a_hint);

	// The bar a_hint is on, after building it hidden while it is undefined
	// and adding it last to the bar a_menu draws. A click on it calls
	// a_clicked. Nothing while no bar takes it or the movie refuses it.
	[[nodiscard]] std::optional<Roles::Bars::Hints> Placed(RE::ExamineMenu& a_menu, Value& a_hint,
		Scaleform::GFx::FunctionHandler& a_clicked);

	// Writes a_hint's word and whether it shows and answers, and
	// menuObj.allowRepair as whether it shows, so a movie that shares T
	// between REPAIR and RENAME hides its RENAME. A RENAME hint of a_bar
	// gives way to it too, see RENAME_GIVES_WAY. Then the movie redraws its
	// buttons.
	void Show(RE::ExamineMenu& a_menu, Value& a_hint, Roles::Bars::Hints& a_bar, std::string_view a_word, bool a_shown,
		bool a_enabled);

	// Hides a_hint, when it is on a bar, and shows the RENAME NEC hid.
	void Withdraw(RE::ExamineMenu& a_menu, Value& a_hint);

	// Forgets the RENAME NEC hid, for a new inspect screen.
	void ForgetRename();
}
