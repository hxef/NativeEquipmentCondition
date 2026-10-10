#include "UI/Repair/Workbench/Label.h"

#include "Core/Text/Text.h"
#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Repair/Workbench/Display.h"
#include "UI/Repair/Workbench/Workbench.h"
#include "UI/Roles/Bars.h"

#include <span>
#include <string>
#include <string_view>

namespace Workbench
{
	namespace
	{
		using Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// The CURRENT MODS heading
		// -------------------------------------------------------------------

		// Hides the CURRENT MODS heading, or shows it again. An item too worn
		// to modify has its slots closed and an item no mod fits has none, so
		// for either one the heading would be wrong. Hiding the field keeps the
		// translated words for when the item is back at full condition.
		void ShowModsLabel(RE::ExamineMenu* a_menu, bool a_shown)
		{
			Value panel;
			Value label;
			if (!a_menu || !a_menu->menuObj.IsObject() ||
				!a_menu->menuObj.GetMember("ModSlotBase_mc"sv, &panel) || !panel.IsObject() ||
				!panel.GetMember("SlotsLabel_tf"sv, &label) || !label.IsObject()) {
				return;
			}
			label.SetMember("visible"sv, Value(a_shown));
		}

		// -------------------------------------------------------------------
		// The REPAIR button's word
		// -------------------------------------------------------------------

		// Called by a bar's list of buttons once for each. The bench's REPAIR
		// button is private to its movie, so it is known by the word it shows,
		// either one, and given the word it should show. The word stays until
		// the next bench movie loads, empty before Label first runs.
		class Wording final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				Value text;
				if (a_params.argCount < 1 || !a_params.args[0].IsObject() ||
					!a_params.args[0].GetMember("ButtonText"sv, &text) || !text.IsString()) {
					return;
				}
				const std::string_view shown = text.GetString();
				if (shown != word && (shown == REPAIR_WORD || shown == mend)) {
					a_params.args[0].SetMember("ButtonText"sv, Value(word.c_str()));
					changed = true;
				}
			}

			std::string mend;
			std::string word;
			bool        changed{ false };
		};

		// Lives as long as the plugin, see Flash.h.
		Wording g_wording;
	}

	void Announce(RE::ExamineMenu* a_menu, const Selection& a_selection)
	{
		if (!a_menu || !a_menu->menuObj.IsObject()) {
			return;
		}
		a_menu->menuObj.SetMember("allowRepair"sv, Value(a_selection.Worn()));
		ShowModsLabel(a_menu, !a_selection.TooWorn() && !NoModFits(a_selection));
	}

	void Label(RE::ExamineMenu* a_menu, const Selection& a_selection)
	{
		if (!a_menu || !a_selection.object) {
			return;
		}

		const bool trifling = a_selection.Trifling();
		g_wording.mend = Text::MendButton();
		g_wording.word = trifling ? g_wording.mend : std::string{ REPAIR_WORD };
		g_wording.changed = false;
		Roles::Bars::ForEach(*a_menu, g_wording);

		if (g_wording.changed) {
			TraceLog::Line("menu", "Workbench button reads {:s} for {:s} at {:d}%",
				trifling ? "MEND" : "REPAIR", a_selection.Name(), a_selection.percent);
		}
	}

	// -------------------------------------------------------------------
	// The word kept each frame
	// -------------------------------------------------------------------

	void Relabel(Scaleform::GFx::Movie& a_movie)
	{
		if (g_wording.word.empty() || !Repairs()) {
			return;
		}
		auto* const menu = OpenBench();
		auto* const bar = menu && menu->uiMovie.get() == &a_movie ? menu->buttonHintBar.get() : nullptr;
		if (!bar) {
			return;
		}

		g_wording.changed = false;
		Value visit;
		a_movie.CreateFunction(&visit, &g_wording);
		Flash::Call(bar->sourceButtons, "forEach", std::span{ &visit, 1 });

		if (g_wording.changed) {
			TraceLog::Once("menu", "Workbench button lost its word on a fresh list, so it reads {:s} again",
				g_wording.word == REPAIR_WORD ? "REPAIR" : "MEND");
		}
	}

	void ForgetWord()
	{
		g_wording.mend.clear();
		g_wording.word.clear();
	}
}
