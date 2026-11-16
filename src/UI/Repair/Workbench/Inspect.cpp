#include "UI/Repair/Workbench/Inspect.h"

#include "Condition/Condition.h"
#include "Core/BenchRow.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings/Settings.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/MessageBox.h"
#include "UI/Repair/SelectedItem.h"
#include "UI/Repair/Workbench/Bench.h"
#include "UI/Repair/Workbench/Cost.h"
#include "UI/Repair/Workbench/InspectHint.h"
#include "UI/Repair/Workbench/Job.h"
#include "UI/Repair/Workbench/Lock.h"
#include "UI/Repair/Workbench/SpareBox.h"
#include "UI/Repair/Workbench/Spares.h"

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace Workbench::Inspect
{
	namespace
	{
		// -------------------------------------------------------------------
		// The screen's state and the click
		// -------------------------------------------------------------------

		// The user event T and Y send.
		constexpr std::string_view HINT_EVENT = "YButton"sv;

		// The frames a fresh inspect screen waits for a bar to take REPAIR,
		// counted only while a row is highlighted.
		constexpr std::uint32_t MOST_FRAMES = 30;

		// Plain C++ only, since the movie keeps no member NEC adds. Forget
		// resets it for each new screen.
		bool          g_decided = false;    // REPAIR was decided on this screen
		std::uint32_t g_frames = 0;         // frames with a row waited for a bar
		bool          g_keyTaken = false;   // NEC took the key's press
		bool          g_noBarSaid = false;  // the trace said no bar took REPAIR

		// What the key place's slot held before NEC, and whether it still runs.
		REL::Relocation<void (*)(RE::BSInputEventUser*, const RE::ButtonEvent*)> _OnButtonEvent;
		CallPatch::LinkBase                                                      g_keyLink;

		enum class By
		{
			kKey,
			kClick,
		};

		void Press(RE::ExamineMenu& a_menu, By a_by);

		// The native function the hint calls when clicked, which lives as long
		// as the plugin, see Flash.h.
		class Clicked final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params&) override
			{
				// A click is handled inside the movie, where it is not safe to
				// open another menu.
				if (const auto* tasks = F4SE::GetTaskInterface()) {
					tasks->AddTask([] {
						if (auto* menu = OpenBench(); PipboyInspect(menu)) {
							Press(*menu, By::kClick);
						}
					});
				}
			}
		};

		Clicked g_clicked;

		// -------------------------------------------------------------------
		// The rule
		// -------------------------------------------------------------------

		// Where REPAIR stands for the item on show, and the trace that says
		// so.
		struct Offer
		{
			bool        shown{ false };
			bool        enabled{ false };
			std::string word{ REPAIR_WORD };
			std::string said;
		};

		// The trace for a row NEC gives no condition, such as aid or junk.
		[[nodiscard]] std::string NoCondition(const RE::InventoryUserUIInterfaceEntry& a_row)
		{
			return std::format("Pip-Boy inspect offers no REPAIR on {:s}, NEC gives it no condition", SelectedItem::RowName(a_row));
		}

		// iPipboyRepair 1: REPAIR shows while bSpareRepair is on, the item is
		// worn, the bench can rebuild it, as a spare's gain needs, and the
		// player carries a spare the box of spares would list.
		[[nodiscard]] Offer WithSpares(const RE::InventoryUserUIInterfaceEntry& a_row)
		{
			Offer      out;
			const auto item = Selection{ SelectedItem::Read(a_row) };
			if (!item.object) {
				out.said = NoCondition(a_row);
				return out;
			}
			const auto none = [&](std::string_view a_why) {
				out.said = std::format("Pip-Boy inspect offers no REPAIR on {:s} at {:s}%, {:s}", item.Name(), Condition::CardText(item.tenths), a_why);
				return out;
			};
			if (!item.Worn()) {
				return none("it is at full");
			}
			if (!Settings::bSpareRepair.GetValue()) {
				return none("bSpareRepair is false");
			}
			if (PriceAsLastRead(item).units == 0) {
				return none("nothing to rebuild it from");
			}
			const auto spares = SparesCarried(item);
			if (spares == 0) {
				return none("no spare carried");
			}
			out.shown = true;
			out.enabled = true;
			out.said = std::format("Pip-Boy inspect shows REPAIR on {:s} at {:s}%, {:d} {:s} carried, as spares only", item.Name(),
				Condition::CardText(item.tenths), spares, spares == 1 ? "spare" : "spares");
			return out;
		}

		// iPipboyRepair 2: as at a bench. MEND for an item worn so little
		// that repairing it is free, else REPAIR while the bench can rebuild
		// it, greyed while no level is above it, as the bench greys it.
		[[nodiscard]] Offer AsBench(const RE::InventoryUserUIInterfaceEntry& a_row)
		{
			Offer      out;
			const auto item = Selection{ SelectedItem::Read(a_row) };
			if (!item.object) {
				out.said = NoCondition(a_row);
				return out;
			}
			const auto none = [&](std::string_view a_why) {
				out.said = std::format("Pip-Boy inspect offers no REPAIR on {:s} at {:s}%, {:s}", item.Name(), Condition::CardText(item.tenths), a_why);
				return out;
			};
			if (!item.Worn()) {
				return none("it is at full");
			}
			const bool mend = item.Trifling();
			if (!mend && PriceAsLastRead(item).units == 0) {
				return none("nothing to rebuild it from");
			}
			const auto spares = SparesCarried(item);
			out.shown = true;
			out.enabled = mend || !Above(item).empty();
			out.word = mend ? Text::MendButton() : std::string{ REPAIR_WORD };
			out.said = std::format("Pip-Boy inspect shows {:s} on {:s} at {:s}%, {:d} {:s} carried, as a workbench", mend ? "MEND" : "REPAIR",
				item.Name(), Condition::CardText(item.tenths), spares, spares == 1 ? "spare" : "spares");
			return out;
		}

		// -------------------------------------------------------------------
		// The press
		// -------------------------------------------------------------------

		// Whether a box is up over the screen, or a repair's box is on its
		// way, so a double click, or T then a click, opens only 1. NEC's own
		// box counts from the moment it is asked, a frame before it opens.
		[[nodiscard]] bool BoxOpen()
		{
			const auto* ui = RE::UI::GetSingleton();
			return InHand().box || MessageBox::AnyAsked() || !ui || ui->GetMenuOpen<RE::MessageBoxMenu>() || ui->GetMenuOpen<RE::ExamineConfirmMenu>();
		}

		// iPipboyRepair 1: the bench's repair lock, then the box of spares.
		// A late press on an item with nothing to rebuild it from, which shows
		// no REPAIR, only says so in the trace.
		void PressSpares(const Selection& a_item)
		{
			if (!a_item.Worn()) {
				return;
			}
			const auto priced = PriceOf(a_item);
			if (priced.units == 0) {
				TraceLog::Line("menu", "Pip-Boy inspect offers no REPAIR on {:s} at {:s}%, nothing to rebuild it from", a_item.Name(),
					Condition::CardText(a_item.tenths));
				return;
			}
			if (!LockStops(a_item, priced)) {
				AskWhichSpare(*a_item.object, a_item.extra, priced);
			}
		}

		// The key or a click on REPAIR, always for the item on show, decided
		// again first.
		void Press(RE::ExamineMenu& a_menu, By a_by)
		{
			if (!RepairsIn(&a_menu)) {
				return;
			}
			Decide(a_menu);
			const auto hint = FindHint(a_menu);
			if (!Offered(hint)) {
				return;
			}
			if (BoxOpen()) {
				TraceLog::Line("menu", "Pip-Boy inspect ignored a press while a box is open");
				return;
			}
			const auto item = Inspected(&a_menu);
			TraceLog::Line("menu", "Pip-Boy inspect {:s} pressed by {:s} on {:s} at {:s}%",
				Flash::String(hint, "ButtonText"sv) == REPAIR_WORD ? "REPAIR" : "MEND", a_by == By::kKey ? "the key YButton" : "a click",
				item.Name(), Condition::CardText(item.tenths));
			if (PipboyRepair() == 1) {
				PressSpares(item);
			} else if (PipboyRepair() == 2) {
				BenchPress(&a_menu, Repairing(&a_menu));
			}
		}

		// The key's press, its held repeats and its release, each marked
		// handled, so none reaches the Pip-Boy under the screen, where T is
		// LEVEL UP or PERK CHART. The release presses REPAIR. True when NEC
		// took the event.
		bool TookKey(RE::ExamineMenu& a_menu, RE::ButtonEvent& a_event)
		{
			if (a_event.QJustPressed()) {
				g_keyTaken = Settings::NecOn() && PipboyInspect(&a_menu) && Offered(FindHint(a_menu));
			}
			if (!g_keyTaken) {
				return false;
			}
			a_event.handled = RE::InputEvent::HANDLED_RESULT::kStop;
			// Held repeats never get here, since the menu's ShouldHandleEvent
			// drops them, so this only guards against them.
			if (a_event.QPressed()) {
				return true;
			}
			g_keyTaken = false;
			Press(a_menu, By::kKey);
			return true;
		}

		// Every button the inspect screen takes. The vanilla slot handles the
		// zoom keys and hands the rest to the movie, which marks each one
		// handled, so NEC marks a key it takes itself. The event comes as
		// const, but the vanilla slot marks it too.
		void OnButtonEventHk(RE::BSInputEventUser* a_this, const RE::ButtonEvent* a_event)
		{
			if (!g_keyLink.Live() || !a_event || a_event->QUserEvent() != HINT_EVENT ||
				!TookKey(*static_cast<RE::ExamineMenu*>(a_this), const_cast<RE::ButtonEvent&>(*a_event))) {
				_OnButtonEvent(a_this, a_event);
			}
		}
	}

	// -------------------------------------------------------------------
	// Install and the decision
	// -------------------------------------------------------------------

	void Install(bool a_benchPatched)
	{
		const auto& input = RE::ExamineMenu::VTABLE[1];
		const auto  key = CallPatch::PatchSlot(input, { 0x08, 0x08 }, OnButtonEventHk, "inspect key", Part::kPipboyRepairs, true, &g_keyLink);
		_OnButtonEvent = key.value_or(0);
		if (!a_benchPatched || !key) {
			REX::WARN("The Pip-Boy's inspect screen stays as it was, with no repairs from NEC.");
			return;
		}

		// As the settings stand at the start. The Settings line gives every
		// change from the MCM page.
		if (Settings::iPipboyRepair.GetValue() == 1 && !Settings::bSpareRepair.GetValue()) {
			REX::INFO("The Pip-Boy's inspect screen repairs nothing, since iPipboyRepair is 1 and bSpareRepair is false.");
		} else if (PipboyRepair() == 1) {
			REX::INFO("The Pip-Boy's inspect screen repairs with a spare.");
		} else if (PipboyRepair() == 2) {
			REX::INFO("The Pip-Boy's inspect screen repairs as a workbench does, with components from the player's inventory.");
		} else if (PipboyRepair() == 0) {
			REX::INFO("The Pip-Boy's inspect screen offers no REPAIR, since iPipboyRepair is {:d}.", Settings::iPipboyRepair.GetValue());
		}
	}

	void Forget()
	{
		g_decided = false;
		g_frames = 0;
		g_keyTaken = false;
		g_noBarSaid = false;
		ForgetRename();
	}

	bool Decide(RE::ExamineMenu& a_menu)
	{
		// The raw key, so the trace can say bSpareRepair is false.
		const auto setting = Settings::iPipboyRepair.GetValue();
		if (!Settings::NecOn() || (setting != 1 && setting != 2) || !PipboyInspect(&a_menu) || !a_menu.uiMovie ||
			!a_menu.menuObj.IsObject()) {
			return false;
		}
		if (!Repairs() || !KeyLive()) {
			auto hint = FindHint(a_menu);
			Withdraw(a_menu, hint);
			g_decided = true;
			return false;
		}
		// The row before the hint, since a screen that keeps no row runs
		// this every frame, and FindHint walks the movie.
		const auto* row = BenchRow::Highlighted(a_menu);
		if (!row) {
			return false;
		}
		auto hint = FindHint(a_menu);
		auto bar = Placed(a_menu, hint, g_clicked);
		if (!bar) {
			return true;
		}

		const auto offer = setting == 1 ? WithSpares(*row) : AsBench(*row);
		Show(a_menu, hint, *bar, offer.word, offer.shown, offer.enabled);
		if (!offer.said.empty()) {
			TraceLog::Line("menu", "{:s}", offer.said);
		}
		g_decided = true;
		return false;
	}

	void OnFrame(Scaleform::GFx::Movie& a_movie)
	{
		if (g_decided) {
			return;
		}
		const auto setting = Settings::iPipboyRepair.GetValue();
		if (!Settings::NecOn() || (setting != 1 && setting != 2)) {
			return;
		}
		auto* menu = OpenBench();
		if (!menu || menu->uiMovie.get() != &a_movie || !menu->inspectMode || !PipboyInspect(menu)) {
			return;
		}
		if (Decide(*menu) && ++g_frames >= MOST_FRAMES && !g_noBarSaid) {
			g_noBarSaid = true;
			g_decided = true;
			TraceLog::Line("menu", "Pip-Boy inspect found no bar for REPAIR, so T stays vanilla");
		}
	}

	bool KeyLive()
	{
		return g_keyLink.Live();
	}
}
