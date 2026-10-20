#include "UI/Repair/VendorRepair/VendorRepair.h"

#include "Condition/Repair.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/ItemCards.h"
#include "Core/TraceLog.h"
#include "UI/Repair/VendorRepair/Button.h"
#include "UI/Repair/VendorRepair/Payment.h"
#include "UI/Repair/VendorRepair/Quote.h"
#include "UI/Repair/VendorRepair/Retry.h"
#include "UI/Repair/VendorRepair/Stock.h"
#include "UI/Repair/VendorRepair/Upkeep.h"

#include <cstdint>
#include <format>
#include <string>
#include <utility>

namespace VendorRepair
{
	namespace
	{
		// -------------------------------------------------------------------
		// The barter screen's hooks
		// -------------------------------------------------------------------

		// What the game does before this file's hooks. The screen has a
		// function table as a menu and one as an input user.
		REL::Relocation<RE::UI_MESSAGE_RESULTS (*)(RE::BarterMenu*, RE::UIMessage&)> _ProcessMessage;
		REL::Relocation<bool (*)(RE::BarterMenu*, const RE::BSFixedString&)>         _OnButtonEventRelease;
		REL::Relocation<void (*)(RE::BarterMenu*, std::int32_t, bool, std::int32_t)> _UpdateItemPickpocketInfo;
		REL::Relocation<void (*)(RE::BarterMenu*, bool)>                             _UpdateList;
		REL::Relocation<bool (*)(RE::BSInputEventUser*, const RE::InputEvent*)>      _ShouldHandleEvent;

		// Whether each of the 4 places still runs, see CallPatch::PatchSlot,
		// and the input place, which trader repairs work without. The messages
		// and list hooks ask Runs, as those 2 places can wait for the screen's
		// next open.
		CallPatch::LinkBase g_messagesLink;
		CallPatch::LinkBase g_keyLink;
		CallPatch::LinkBase g_highlightLink;
		CallPatch::LinkBase g_listLink;
		CallPatch::LinkBase g_inputLink;

		// Whether NEC saw this screen open. Not while barter messages or barter
		// list waits for the screen's next open, see Core/CallPatch/MenuWait.h,
		// since the screen may still hold the last one's state, so the other
		// hooks give the game's result until then.
		bool Seen()
		{
			return g_messagesLink.Live() && g_listLink.Live();
		}

		// A UI replacer can use the key in button combinations of its own and
		// leave the key's release to the game, as one can with LB on a pad. So
		// a release presses REPAIR only when no other button and no stick went
		// down while the key was held. g_releaseJoined holds that answer for
		// the key's last release, read by the release hook just after, since
		// the input place sees each event first.
		bool g_keyDown = false;
		bool g_keyJoined = false;
		bool g_releaseJoined = false;

		// A new screen starts with the key up.
		void ForgetKey()
		{
			g_keyDown = false;
			g_keyJoined = false;
			g_releaseJoined = false;
		}

		// The key going down and up, and any other button or stick going down
		// between. A release is told by the same test IMenu::OnButtonEvent
		// uses before it calls OnButtonEventRelease.
		void Track(const RE::InputEvent& a_event)
		{
			if (const auto* stick = a_event.As<RE::ThumbstickEvent>()) {
				const auto moved = stick->prevDir == RE::DIRECTION_VAL::kNone && stick->currDir != RE::DIRECTION_VAL::kNone;
				g_keyJoined = g_keyJoined || (g_keyDown && moved);
				return;
			}
			const auto* button = a_event.As<RE::ButtonEvent>();
			if (!button) {
				return;
			}
			const auto key = button->QUserEvent() == HINT_EVENT;
			if (key && button->QJustPressed()) {
				g_keyDown = true;
				g_keyJoined = false;
			} else if (key && !button->QPressed() && button->QHeldDownSecs() >= 0.0F) {
				g_releaseJoined = g_keyDown && g_keyJoined;
				g_keyDown = false;
				g_keyJoined = false;
			} else if (!key && button->QJustPressed()) {
				g_keyJoined = g_keyJoined || g_keyDown;
			}
		}

		// Asked about every input before the screen's movie sees it. It only
		// watches and gives the game's own answer, which is no while the
		// screen opens or is suppressed.
		bool ShouldHandleEventHk(RE::BSInputEventUser* a_this, const RE::InputEvent* a_event)
		{
			if (g_inputLink.Live()) {
				Track(*a_event);
			}
			return _ShouldHandleEvent(a_this, a_event);
		}

		// The movie telling code where the highlight landed. It asks for the
		// pickpocket odds whenever the highlight lands or the lists are
		// rebuilt, and on every move of the quantity slider, for the item the
		// slider is open on.
		void UpdateItemPickpocketInfoHk(RE::BarterMenu* a_menu, std::int32_t a_index, bool a_inContainer,
			std::int32_t a_count)
		{
			_UpdateItemPickpocketInfo(a_menu, a_index, a_inContainer, a_count);
			if (!HighlightLive()) {
				return;
			}
			Highlight(a_index, a_inContainer);
			Refresh(a_menu);
		}

		// A list rebuilt, which can come with a fresh list of hints for the
		// bar. What the trader restocks with is forgotten before the game
		// builds their side again, see ForgetStock, so a highlight the rebuild
		// sets off reads it again.
		void UpdateListHk(RE::BarterMenu* a_menu, bool a_inContainer)
		{
			if (!g_listLink.Runs() || !g_messagesLink.Live()) {
				_UpdateList(a_menu, a_inContainer);
				return;
			}
			if (a_inContainer) {
				ForgetStock();
			}
			_UpdateList(a_menu, a_inContainer);
			Refresh(a_menu);
		}

		// The key. The barter screen's own code answers 6 of the 17 buttons
		// menu mode names, after the movie has seen each, and passes the rest
		// on. A greyed REPAIR is answered too, which is how the player hears
		// why it is greyed, and a screen with no REPAIR leaves the key to the
		// game. So does a release after another button or a stick went down,
		// see g_keyJoined.
		bool OnButtonEventReleaseHk(RE::BarterMenu* a_menu, const RE::BSFixedString& a_event)
		{
			if (g_keyLink.Live() && Seen() && a_event == HINT_EVENT && Offered(a_menu)) {
				if (std::exchange(g_releaseJoined, false)) {
					TraceLog::Line("menu", "REPAIR's key came up at {:s} after another button or a stick went down while it was held, so REPAIR opens nothing and the key goes on to the game",
						Trader(a_menu));
				} else if (Press()) {
					return true;
				}
			}
			return _OnButtonEventRelease(a_menu, a_event);
		}

		// A new screen forgets the last one before the game builds its lists.
		// The screen being put away: the game's own handling stops the markup
		// on every price, so the Pip-Boy is told about a repair a moment after,
		// through F4SE's task queue, once for each kind repaired.
		RE::UI_MESSAGE_RESULTS ProcessMessageHk(RE::BarterMenu* a_menu, RE::UIMessage& a_message)
		{
			// Only the message that opens the screen brings a waiting place
			// back, as the last screen is forgotten there.
			const auto opens = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			if (opens ? !g_messagesLink.Runs() : !g_messagesLink.Live()) {
				return _ProcessMessage(a_menu, a_message);
			}
			if (opens) {
				Forget();
				ForgetKey();
				ForgetQuestion();
				ForgetWait();
			}
			const auto result = _ProcessMessage(a_menu, a_message);
			if (*a_message.type == RE::UI_MESSAGE_TYPE::kHide) {
				const auto* tasks = F4SE::GetTaskInterface();
				for (const auto owed : TakeCardsOwed()) {
					if (tasks) {
						tasks->AddTask([owed] { ItemCards::Refresh(owed); });
					}
				}
			}
			return result;
		}

		// -------------------------------------------------------------------
		// The game's message boxes
		// -------------------------------------------------------------------

		// Set once Install writes all 4 barter places, so Load adds no sink
		// while another mod holds any of them.
		bool g_installed = false;

		// Refreshes REPAIR whenever a message box opens or closes, since the
		// game opens its trade and INVEST boxes over the barter screen without
		// a call NEC hooks. Only queued here, since the game sends this while
		// it works on its menus.
		class BoxSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			F4_HEAP_REDEFINE_NEW(BoxSink);

		private:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event.menuName == RE::MessageBoxMenu::MENU_NAME) {
					if (const auto* tasks = F4SE::GetTaskInterface()) {
						tasks->AddTask([] {
							if (auto* menu = OpenBarter(); menu && HighlightLive()) {
								Refresh(menu);
							}
						});
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	// -------------------------------------------------------------------
	// Install and Load
	// -------------------------------------------------------------------

	void Install()
	{
		REL::Relocation<std::uintptr_t> menu{ RE::BarterMenu::VTABLE[0] };
		REL::Relocation<std::uintptr_t> input{ RE::BarterMenu::VTABLE[1] };

		const auto messages = CallPatch::PatchSlot(menu, 0x03, ProcessMessageHk, "barter messages", Part::kNone, true, &g_messagesLink);
		const auto key = CallPatch::PatchSlot(menu, 0x0F, OnButtonEventReleaseHk, "barter key", Part::kNone, true, &g_keyLink);
		const auto highlight = CallPatch::PatchSlot(menu, 0x1F, UpdateItemPickpocketInfoHk, "barter highlight", Part::kNone, true,
			&g_highlightLink);
		const auto list = CallPatch::PatchSlot(menu, 0x20, UpdateListHk, "barter list", Part::kNone, true, &g_listLink);
		if (!messages || !key || !highlight || !list) {
			return;
		}
		_ProcessMessage = *messages;
		_OnButtonEventRelease = *key;
		_UpdateItemPickpocketInfo = *highlight;
		_UpdateList = *list;
		g_installed = true;

		// Noted only when the 4 can go in. Trader repairs work without it, so
		// another mod there turns nothing off, see Part::kTraderKeys.
		_ShouldHandleEvent = CallPatch::PatchSlot(input, 0x01, ShouldHandleEventHk, "barter input", Part::kTraderKeys, true, &g_inputLink).value_or(0);

		REX::INFO("Traders who restock {:d} rows of weapons with one row in {:d} a weapon, "
				  "or {:d} rows of weapons, ammunition, grenades and mines with one row in {:d} one of those, "
				  "repair weapons for caps. The same counts of armor and power armor pieces buy armor repairs, "
				  "and of clothing alone clothing repairs, each kind as far as its own rows go, keyed to {:s}.",
			MANY_ROWS, MANY_SHARE, SOME_ROWS, SOME_SHARE, HINT_EVENT);
		for (const auto trade : { Trade::kWeapons, Trade::kArmor, Trade::kClothing }) {
			std::string reach;
			for (std::size_t rows = 1; rows <= FullRows(trade); rows++) {
				reach += std::format("{:s}{:d}:{:d}%", reach.empty() ? "" : " ", rows, Reach(trade, rows));
			}
			REX::INFO("How far they take {:s} back, by rows of that kind in stock, {:s}", Named(trade), reach);
		}
		REX::INFO("An item at nothing owes a trader {:.2f} times what it is worth, and at each level {:s}",
			Repair::Debt(0, Scaled(WRECK_MULTIPLE)), Repair::Ladder(Scaled(WRECK_MULTIPLE)));

		// RestockHk reads bVendorRepair and bSpawnCondition on every restock.
		InstallUpkeep();
	}

	void Load()
	{
		// Once, since UI lasts as long as the game.
		static bool registered = false;
		auto*       ui = RE::UI::GetSingleton();
		if (registered || !g_installed || !ui) {
			return;
		}
		ui->RegisterSink<RE::MenuOpenCloseEvent>(new BoxSink());
		registered = true;
	}

	bool HighlightLive()
	{
		return g_highlightLink.Live() && Seen();
	}
}
