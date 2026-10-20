#include "Core/CallPatch/MenuWait.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

// A wait ends in a cut only once its menu opened and closed with no call
// reaching NEC's hook, so the 3 menus' opens and closes are counted. The game
// sends a menu's open once the menu has handled the message that opens it,
// and its close once the menu has left the screen, so every call 1 use of the
// menu makes comes before that use's close. A menu already open that is shown
// again sends neither.
//
// The sink counts on the thread that runs the menus and a recheck reads the
// counts on its own thread, so the counts are atomic.
namespace CallPatch
{
	namespace
	{
		struct Menu
		{
			std::string_view           name;   // as the game names it
			std::string_view           words;  // as NEC.log names it
			std::atomic<std::uint32_t> opens{ 0 };
			std::atomic<std::uint32_t> closes{ 0 };
		};

		constexpr std::size_t BARTER = 0;
		constexpr std::size_t WORKBENCH = 1;
		constexpr std::size_t BOX = 2;

		std::array<Menu, 3> g_menus{ {
			{ RE::BarterMenu::MENU_NAME, "barter screen"sv },
			{ RE::ExamineMenu::MENU_NAME, "workbench or inspect screen"sv },
			{ RE::ExamineConfirmMenu::MENU_NAME, "confirmation box"sv },
		} };

		// The places every open of their menu calls, read off the game. The
		// message that opens the barter screen comes to barter messages and
		// rebuilds both lists through barter list. A workbench's first frame
		// fills its item list through bench item list, which hands the rows
		// to Flash at item list refresh, and so does the Pip-Boy's inspect
		// screen, the same menu. The message that opens the box comes to
		// confirm box messages. Each hook asks its link's Runs, see Link.h,
		// the 2 message hooks only for that message.
		//
		// A place here must have pieces that need nothing a wait can hold
		// off, see the settle in Recheck.cpp, or its wait would read as a
		// cut. So barter screen opens is not here, as the trader's stock
		// needs Trader repairs and Worn loot.
		std::array g_waits{
			MenuWait{ "barter messages"sv, BARTER },
			MenuWait{ "barter list"sv, BARTER },
			MenuWait{ "bench item list"sv, WORKBENCH },
			MenuWait{ "item list refresh"sv, WORKBENCH },
			MenuWait{ "confirm box messages"sv, BOX },
		};

		class Sink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			F4_HEAP_REDEFINE_NEW(Sink);

		private:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				for (auto& menu : g_menus) {
					if (a_event.menuName == menu.name) {
						(a_event.opening ? menu.opens : menu.closes).fetch_add(1, std::memory_order_release);
						break;
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	MenuWait* MenuWaitOf(const Place& a_place)
	{
		const auto it = std::ranges::find(g_waits, std::string_view{ a_place.what }, &MenuWait::what);
		return it != g_waits.end() ? &*it : nullptr;
	}

	MenuWait* WaitingOf(const Place& a_place)
	{
		auto* wait = MenuWaitOf(a_place);
		return wait && wait->due != 0 ? wait : nullptr;
	}

	// A menu open at this moment does not count, as its close comes before
	// the one due.
	void Wait(MenuWait& a_wait, std::vector<Owner> a_line, bool a_unread)
	{
		a_wait.due = g_menus[a_wait.menu].opens.load(std::memory_order_acquire) + 1;
		a_wait.unread = a_unread;
		a_wait.line = std::move(a_line);
	}

	std::vector<Owner> Done(MenuWait& a_wait)
	{
		a_wait.due = 0;
		a_wait.unread = false;
		return std::exchange(a_wait.line, {});
	}

	bool MenuUsed(const MenuWait& a_wait)
	{
		return g_menus[a_wait.menu].closes.load(std::memory_order_acquire) >= a_wait.due;
	}

	std::string_view MenuWords(const MenuWait& a_wait)
	{
		return g_menus[a_wait.menu].words;
	}

	// UI lasts as long as the game. The first recheck comes as the main menu
	// shows or every file has loaded, before any of the 3 menus can open.
	void CountMenus()
	{
		static std::atomic<bool> registered{ false };
		auto*                    ui = RE::UI::GetSingleton();
		if (!ui || registered.exchange(true)) {
			return;
		}
		ui->RegisterSink<RE::MenuOpenCloseEvent>(new Sink());
	}
}
