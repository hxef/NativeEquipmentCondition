#include "UI/Repair/VendorRepair/Button.h"

#include "Core/Settings.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog.h"
#include "UI/Repair/RepairPrompt.h"
#include "UI/Repair/VendorRepair/Payment.h"
#include "UI/Repair/VendorRepair/Quote.h"
#include "UI/Repair/VendorRepair/Retry.h"
#include "UI/Repair/VendorRepair/Stock.h"
#include "UI/Roles/Bars.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace VendorRepair
{
	namespace
	{
		using Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// The button and where it is kept
		// -------------------------------------------------------------------

		// The button, as the game builds its own.
		constexpr const char*   HINT_CLASS = "Shared.AS3.BSButtonHintData";
		constexpr const char*   HINT_LABEL = "$REPAIR";
		constexpr const char*   HINT_KEY = "C";
		constexpr const char*   HINT_PSN = "PSN_L1";
		constexpr const char*   HINT_XENON = "Xenon_L1";
		constexpr std::uint32_t HINT_JUSTIFY = 1;

		// Where the button is kept between presses. The barter screen's class
		// is dynamic, so the plugin can add a member to it, and asking for it
		// back says whether the screen has a button yet.
		constexpr std::string_view HINT_MEMBER = "NECRepairButton"sv;

		// Set while the question is on the screen, so a second press cannot
		// put a second copy of it behind the first.
		bool g_asking = false;

		// Set when a screen refuses the plugin's button. Every barter screen is
		// built the same way, so if one refuses, they all will.
		bool g_refused = false;

		// The native function the button calls when clicked, which lives as
		// long as the plugin, see Flash.h.
		class Pressed final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params&) override
			{
				// A click is handled inside the movie, where it is not safe to
				// open another menu.
				const auto* tasks = F4SE::GetTaskInterface();
				if (tasks) {
					tasks->AddTask([] { Press(); });
				}
			}
		};

		Pressed g_pressed;

		// The button this screen was given, not an object where it has none yet
		// or would not build one.
		[[nodiscard]] Value Hint(RE::BarterMenu* a_menu)
		{
			Value hint;
			if (a_menu && a_menu->menuObj.IsObject()) {
				a_menu->menuObj.GetMember(HINT_MEMBER, &hint);
			}
			return hint;
		}

		// -------------------------------------------------------------------
		// Where REPAIR stands
		// -------------------------------------------------------------------

		// Whether one of the game's message boxes is up, NEC's own question
		// among them. The game puts the barter screen in message box mode for
		// its trade and INVEST boxes, which hides the screen's own hints and
		// runs no NEC hook, so REPAIR goes off with them, see BoxSink in
		// VendorRepair.cpp.
		[[nodiscard]] bool BoxOpen()
		{
			const auto* ui = RE::UI::GetSingleton();
			return ui && ui->GetMenuOpen<RE::MessageBoxMenu>();
		}

		// Where REPAIR stands for the item under the highlight, the one answer
		// the bar, the trace log and a press all use. Checked in this order, so
		// an item past what the trader repairs says so even while a trade is
		// pending.
		enum class Stand
		{
			kOff,
			kPastCeiling,
			kTradePending,
			kUnaffordable,
			kLive,
		};

		[[nodiscard]] Stand StandOf(RE::BarterMenu* a_menu, const Selection& a_selection, std::uint32_t a_ceiling)
		{
			if (!Settings::bVendorRepair.GetValue() || g_asking || !a_menu || BoxOpen() || !Shown(a_selection, a_ceiling)) {
				return Stand::kOff;
			}
			if (a_selection.percent >= a_ceiling) {
				return Stand::kPastCeiling;
			}
			if (TradePending(a_menu)) {
				return Stand::kTradePending;
			}
			return Afforded(Quotes(a_selection, a_ceiling)).empty() ? Stand::kUnaffordable : Stand::kLive;
		}

		// A refused press, explained in the corner with the menus' refusal
		// sound.
		void Refuse(const std::string& a_said)
		{
			RE::SendHUDMessage::ShowHUDMessage(a_said.c_str(), nullptr, true, true);
			RE::UIUtils::PlayMenuSound(RepairPrompt::REFUSED_SOUND);
		}

		// Tells the trace log what the button does for a worn item under the
		// highlight, once for each thing said, see TraceLog::First, so a row
		// passed over again and again is not written again. Silent while a box
		// is up, where REPAIR is off whatever the trader repairs.
		void Tell(RE::BarterMenu* a_menu, const Selection& a_selection, std::uint32_t a_ceiling, Stand a_stand)
		{
			if (!a_selection.Worn() || g_asking || !TraceLog::IsOpen() || BoxOpen()) {
				return;
			}

			const auto trader = Trader(a_menu);
			const auto name = a_selection.Name();
			const auto trade = Named(a_selection.trade);
			switch (a_stand) {
			case Stand::kOff:
				TraceLog::First("menu", "{:s} repairs no {:s}, so REPAIR stays off {:s} at {:d}%",
					trader, trade, name, a_selection.percent);
				break;
			case Stand::kPastCeiling:
				TraceLog::First("menu", "{:s} greys REPAIR on {:s} at {:d}%, since they take {:s} no further than {:d}%",
					trader, name, a_selection.percent, trade, a_ceiling);
				break;
			case Stand::kTradePending:
				TraceLog::First("menu", "{:s} greys REPAIR on {:s} at {:d}%, as {:s}, since a trade is pending",
					trader, name, a_selection.percent, trade);
				break;
			case Stand::kUnaffordable:
				TraceLog::First("menu", "{:s} greys REPAIR on {:s} at {:d}%, as {:s}, since the player cannot pay the smallest step",
					trader, name, a_selection.percent, trade);
				break;
			case Stand::kLive:
				TraceLog::First("menu", "{:s} shows REPAIR on {:s} at {:d}%, as {:s}, up to {:d}%",
					trader, name, a_selection.percent, trade, a_ceiling);
				break;
			}
		}

		// -------------------------------------------------------------------
		// Putting the button on a bar
		// -------------------------------------------------------------------

		// Builds the button and keeps it on the screen. False when the screen
		// refuses it.
		[[nodiscard]] bool Build(RE::BarterMenu* a_menu, Value& a_hint)
		{
			Value pressed;
			a_menu->uiMovie->CreateFunction(&pressed, &g_pressed);

			// The same 6 the game passes when it builds one. The button's own
			// handler is written over afterwards.
			const std::array<Value, 6> made{
				Value(HINT_LABEL),
				Value(HINT_KEY),
				Value(HINT_PSN),
				Value(HINT_XENON),
				Value(HINT_JUSTIFY),
				Value(nullptr),
			};

			a_menu->uiMovie->CreateObject(&a_hint, HINT_CLASS, made.data(),
				static_cast<std::uint32_t>(made.size()));
			if (!a_hint.IsObject()) {
				g_refused = true;
				REX::WARN("The barter screen would not build a button hint, so no trader offers REPAIR.");
				return false;
			}

			// Hidden until Refresh says where REPAIR stands.
			a_hint.SetMember("onTextClick"sv, pressed);
			a_hint.SetMember("ButtonVisible"sv, Value(false));
			a_menu->menuObj.SetMember(HINT_MEMBER, a_hint);
			return true;
		}

		// Where REPAIR stands on the barter screen's bars after a refresh.
		enum class Wired
		{
			kOn,       // on a bar
			kWaiting,  // no bar answers yet
			kOff,      // no screen, or the screen or its list refused the hint
		};

		// Puts the button on the bar the barter screen draws now, since a movie
		// can hand its bar a fresh list later. The hint is built first, so the
		// key answers even when no bar holds it.
		[[nodiscard]] Wired Wire(RE::BarterMenu* a_menu)
		{
			if (g_refused || !a_menu || !a_menu->uiMovie || !a_menu->menuObj.IsObject()) {
				return Wired::kOff;
			}
			auto hint = Hint(a_menu);
			if (!hint.IsObject() && !Build(a_menu, hint)) {
				return Wired::kOff;
			}
			auto bar = Roles::Bars::InUse(*a_menu, hint);
			if (!bar) {
				return Wired::kWaiting;
			}
			if (bar->ours) {
				return Wired::kOn;
			}
			if (Roles::Bars::Add(*bar, hint)) {
				TraceLog::Line("menu", "{:s} offers REPAIR on the bar, keyed to {:s}",
					Trader(a_menu), HINT_EVENT);
				return Wired::kOn;
			}
			// The hint leaves this screen too, so C goes back to the game here
			// as on every later screen.
			hint.SetMember("ButtonVisible"sv, Value(false));
			a_menu->menuObj.SetMember(HINT_MEMBER, Value());
			g_refused = true;
			REX::WARN("The barter screen would not take another button hint, so no trader offers REPAIR.");
			return Wired::kOff;
		}

		// -------------------------------------------------------------------
		// The question
		// -------------------------------------------------------------------

		// Holds the barter screen while the question is up, the way the game
		// does for its own boxes: message box mode hides the screen's own
		// hints, the open box hides REPAIR and takes the keys. g_asking hides
		// REPAIR and stops a second press until the box is up. Hold and
		// ForgetQuestion are its only writers.
		void Hold(RE::BarterMenu* a_menu, bool a_held)
		{
			g_asking = a_held;
			if (a_menu) {
				a_menu->SetMessageBoxMode(a_held);
			}
			Refresh(a_menu);
		}
	}

	// -------------------------------------------------------------------
	// Refreshing and pressing the button
	// -------------------------------------------------------------------

	void Release()
	{
		Hold(OpenBarter(), false);
	}

	void Refresh(RE::BarterMenu* a_menu)
	{
		// Switched off, the screen shows no REPAIR. A screen built on the last
		// one's movie still has the button, so it is hidden, and none is wired.
		if (!Settings::bVendorRepair.GetValue()) {
			if (auto hint = Hint(a_menu); hint.IsObject()) {
				hint.SetMember("ButtonVisible"sv, Value(false));
			}
			return;
		}

		// A bar REPAIR joined can be handed a fresh list without it a few
		// frames later, so the screen is watched a while, see Retry.h.
		const auto wired = Wire(a_menu);
		if (wired != Wired::kWaiting) {
			EndWait(wired == Wired::kOn);
		}
		if (wired == Wired::kOn) {
			WatchBar(*a_menu);
		}
		auto hint = Hint(a_menu);
		if (wired == Wired::kOff || !hint.IsObject()) {
			return;
		}

		// A list the hint showed on can be replaced by one no bar draws yet,
		// so the hint is hidden until a bar takes it.
		if (wired == Wired::kWaiting) {
			hint.SetMember("ButtonVisible"sv, Value(false));
			WaitForBar(*a_menu);
			return;
		}

		// Greyed wherever there is nothing to buy: an item past what this
		// trader repairs, or a smallest step the player cannot pay for. Greyed
		// too while a trade is pending, see Payment.h.
		const auto selection = Selected(a_menu);
		const auto ceiling = Ceiling(a_menu, selection);
		const auto stand = StandOf(a_menu, selection, ceiling);
		hint.SetMember("ButtonVisible"sv, Value(stand != Stand::kOff));
		hint.SetMember("ButtonDisabled"sv, Value(stand != Stand::kOff && stand != Stand::kLive));
		Tell(a_menu, selection, ceiling, stand);
	}

	bool Press()
	{
		auto*      menu = OpenBarter();
		const auto selection = Selected(menu);
		const auto ceiling = Ceiling(menu, selection);
		const auto stand = StandOf(menu, selection, ceiling);
		if (stand == Stand::kOff) {
			return false;
		}

		if (stand == Stand::kPastCeiling) {
			const auto said = Text::RepairCeiling(ceiling, selection.trade);
			Refuse(said);
			TraceLog::Line("menu", "{:s} can take {:s} no further than {:d}%, and it is at {:d}%, saying \"{:s}\"",
				Trader(menu), selection.Name(), ceiling, selection.percent, said);
			return true;
		}

		if (stand == Stand::kTradePending) {
			const auto said = Text::RepairTradePending();
			Refuse(said);
			TraceLog::Line("menu", "{:s} waits for the pending trade before repairing {:s} at {:d}%, saying \"{:s}\"",
				Trader(menu), selection.Name(), selection.percent, said);
			return true;
		}

		TraceLog::Line("menu", "{:s} was asked to repair {:s} at {:d}%, as {:s}, and will go to {:d}%",
			Trader(menu), selection.Name(), selection.percent, Named(selection.trade), ceiling);

		if (stand == Stand::kUnaffordable) {
			const auto said = Text::RepairUnaffordable();
			Refuse(said);
			TraceLog::Line("menu", "{:s} asked for more than the player has for {:s} at {:d}%, saying \"{:s}\"",
				Trader(menu), selection.Name(), selection.percent, said);
			return true;
		}

		const auto               quotes = Afforded(Quotes(selection, ceiling));
		std::vector<std::string> buttons;
		std::string              spelled;
		for (const auto& quote : quotes) {
			buttons.push_back(Text::RepairPrice(quote.level, quote.price));
			spelled += std::format("{:s}{:d}%:{:d}", spelled.empty() ? "" : " ", quote.level, quote.price);
		}

		// How far this trader goes with this kind, above the question, since
		// the buttons stop at the limit and nothing else on the screen says
		// why, and the same trader may repair another kind further.
		const auto over = Text::RepairUpTo(ceiling, selection.trade);
		TraceLog::Line("menu", "{:s} asks how far to repair {:s} at {:d}%, a stack of {:d}, worth {:d}, under \"{:s}\", offering {:s}",
			Trader(menu), selection.Name(), selection.percent, selection.count, selection.worth, over, spelled);

		Hold(menu, true);
		RepairPrompt::Ask(over, selection.Name(), selection.percent, std::move(buttons),
			[quotes, handle = selection.handle, stack = selection.stack](std::size_t a_index) {
				const auto& quote = quotes[a_index];
				TraceLog::Line("menu", "The repair was set to {:d}% for {:d} caps", quote.level, quote.price);
				Pay(quote, handle, stack);
			},
			[] {
				TraceLog::Line("menu", "The repair was called off");
				Release();
			});
		return true;
	}

	void ForgetQuestion()
	{
		g_asking = false;
	}

	bool Offered(RE::BarterMenu* a_menu)
	{
		return Hint(a_menu).IsObject();
	}
}
