#include "UI/Repair/Workbench/InspectHint.h"

#include "Core/Text/Text.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Repair/Workbench/Bench.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace Workbench::Inspect
{
	namespace
	{
		// -------------------------------------------------------------------
		// Finding hints by their word
		// -------------------------------------------------------------------

		// The hint, as the game builds the bench's REPAIR: T on a keyboard,
		// Y on a pad, the user event YButton.
		constexpr const char*   HINT_CLASS = "Shared.AS3.BSButtonHintData";
		constexpr const char*   HINT_KEY = "T";
		constexpr const char*   HINT_PSN = "PSN_Y";
		constexpr const char*   HINT_XENON = "Xenon_Y";
		constexpr std::uint32_t HINT_JUSTIFY = 1;

		// The word of a RENAME hint, which a mod that renames any item can add
		// to the inspect screen, on T as well. While REPAIR shows it is
		// hidden, so T has 1 meaning on screen, and it shows again once REPAIR
		// goes. False leaves RENAME as the movie draws it.
		constexpr std::string_view RENAME_WORD = "$RENAME"sv;
		constexpr bool             RENAME_GIVES_WAY = true;

		// Called by a bar's list once for each hint. Keeps the first one
		// whose word is REPAIR or MEND.
		class Finder final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				if (found.IsObject() || a_params.argCount < 1) {
					return;
				}
				const auto shown = Flash::String(a_params.args[0], "ButtonText"sv);
				if (shown == REPAIR_WORD || shown == mend) {
					found = a_params.args[0];
				}
			}

			std::string mend;
			Value       found;
		};

		// Called by 1 bar's list once for each hint, with the hint and its
		// place in the list. Hiding, it hides the first RENAME hint on show
		// and keeps its place. Showing, it shows only the hint at that place,
		// so a RENAME the movie or another mod hid stays hidden.
		class Renamer final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				if (a_params.argCount < 2 || !Flash::IsAnyNumber(a_params.args[1]) ||
					Flash::String(a_params.args[0], "ButtonText"sv) != RENAME_WORD ||
					Flash::Bool(a_params.args[0], "ButtonVisible"sv) == shown) {
					return;
				}
				const auto at = static_cast<std::uint32_t>(Flash::AsNumber(a_params.args[1]));
				if (hidden ? *hidden != at : shown) {
					return;
				}
				if (Flash::Set(a_params.args[0], "ButtonVisible"sv, Value(shown))) {
					hidden = shown ? std::nullopt : std::optional{ at };
				}
			}

			bool                         shown = false;
			std::optional<std::uint32_t> hidden;  // the place of the RENAME NEC hid on this screen
		};

		// Both live as long as the plugin, see Flash.h.
		Finder  g_finder;
		Renamer g_renamer;

		// Hides a visible RENAME hint of a_bar while REPAIR shows, and shows
		// the one NEC hid once it goes.
		void GiveWay(RE::ExamineMenu& a_menu, Roles::Bars::Hints& a_bar, bool a_repair)
		{
			if (!RENAME_GIVES_WAY || (!a_repair && !g_renamer.hidden)) {
				return;
			}
			g_renamer.shown = !a_repair;
			Value visit;
			a_menu.uiMovie->CreateFunction(&visit, &g_renamer);
			Flash::Call(a_bar.list, "forEach", std::span{ &visit, 1 });
			if (!a_repair) {
				// Forgotten whether it showed again here or is no longer there.
				g_renamer.hidden.reset();
			}
		}
	}

	// -------------------------------------------------------------------
	// The hint
	// -------------------------------------------------------------------

	Value FindHint(RE::ExamineMenu& a_menu)
	{
		g_finder.mend = Text::MendButton();
		Roles::Bars::ForEach(a_menu, g_finder);
		return std::exchange(g_finder.found, Value{});
	}

	bool Offered(const Value& a_hint)
	{
		return Flash::Bool(a_hint, "ButtonVisible"sv) && Flash::Bool(a_hint, "ButtonEnabled"sv);
	}

	std::optional<Roles::Bars::Hints> Placed(RE::ExamineMenu& a_menu, Value& a_hint, Scaleform::GFx::FunctionHandler& a_clicked)
	{
		if (!a_menu.uiMovie) {
			return std::nullopt;
		}
		if (!a_hint.IsObject()) {
			// The same 6 values the game passes for its own hints. The click
			// handler is written over afterwards.
			const std::array<Value, 6> made{
				Value(REPAIR_WORD),
				Value(HINT_KEY),
				Value(HINT_PSN),
				Value(HINT_XENON),
				Value(HINT_JUSTIFY),
				Value(nullptr),
			};
			a_menu.uiMovie->CreateObject(&a_hint, HINT_CLASS, made.data(), static_cast<std::uint32_t>(made.size()));
			if (!a_hint.IsObject()) {
				return std::nullopt;
			}
			Value clicked;
			a_menu.uiMovie->CreateFunction(&clicked, &a_clicked);
			Flash::Set(a_hint, "onTextClick"sv, clicked);
			Flash::Set(a_hint, "ButtonVisible"sv, Value(false));
		}

		auto bar = Roles::Bars::InUse(a_menu, a_hint);
		if (!bar || bar->ours) {
			return bar;
		}
		if (!Roles::Bars::Add(*bar, a_hint)) {
			return std::nullopt;
		}
		TraceLog::Line("menu", "Pip-Boy inspect adds REPAIR to the bar, keyed to YButton");
		return bar;
	}

	void Show(RE::ExamineMenu& a_menu, Value& a_hint, Roles::Bars::Hints& a_bar, std::string_view a_word, bool a_shown, bool a_enabled)
	{
		const std::string word{ a_word };
		Flash::Set(a_hint, "ButtonText"sv, Value(word.c_str()));
		Flash::Set(a_hint, "ButtonEnabled"sv, Value(a_enabled));
		Flash::Set(a_hint, "ButtonVisible"sv, Value(a_shown));
		Flash::Set(a_menu.menuObj, "allowRepair"sv, Value(a_shown));
		GiveWay(a_menu, a_bar, a_shown);
		Flash::Call(a_menu.menuObj, "UpdateButtons");
	}

	void Withdraw(RE::ExamineMenu& a_menu, Value& a_hint)
	{
		if (a_hint.IsObject()) {
			Flash::Set(a_hint, "ButtonVisible"sv, Value(false));
			if (auto bar = Roles::Bars::InUse(a_menu, a_hint)) {
				GiveWay(a_menu, *bar, false);
			}
		}
		Flash::Set(a_menu.menuObj, "allowRepair"sv, Value(false));
		Flash::Call(a_menu.menuObj, "UpdateButtons");
	}

	void ForgetRename()
	{
		g_renamer.hidden.reset();
	}
}
