#include "Session.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/CraftingPerks/CraftingPerks.h"
#include "Condition/Materials/Materials.h"
#include "Condition/Provenance/Provenance.h"
#include "Condition/WeaponWear/WeaponWear.h"
#include "Core/ScriptEvents.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog.h"
#include "Gameplay/FireRate/FireRate.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "Gameplay/InHand.h"
#include "Gameplay/Jam.h"
#include "Papyrus/Papyrus.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Repair/Columns/Columns.h"

#include <atomic>
#include <chrono>

namespace Session
{
	namespace
	{
		// -------------------------------------------------------------------
		// The main menu and loading screens
		// -------------------------------------------------------------------

		// Set while a game could begin with no save load or new game message,
		// as a coc from the main menu does. 0 means nothing is due, 1 that the
		// main menu opened and 2 that a loading screen opened after it. A quit's
		// own loading screen opens before the main menu, so it closes at 1.
		std::atomic<int> g_unmeasured{ 0 };

		// The main menu opening drops the last session's readings too, since a coc
		// from there starts a game with no load or new game message. A loading
		// screen that opened after the main menu measures the load order again
		// when it closes, unless a save load or new game came first. Every
		// loading screen reads the scrap lists again as it closes, so wear
		// follows an item a quest script added during play before the next bench
		// visit, see Materials::ReadScrapLists. Queued, since the game sends this
		// while it works on its menus.
		class MainMenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			F4_HEAP_REDEFINE_NEW(MainMenuSink);

		private:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				// Events pushed under an inventory lock, as a loot roll's, wait
				// for a kick, and every menu that opens or closes is one.
				ScriptEvents::KickIfWaiting();
				if (a_event.opening && a_event.menuName == RE::MainMenu::MENU_NAME) {
					g_unmeasured = 1;
					// Here, before any task runs, so a write's work queued in the
					// game before is dropped.
					ScriptEvents::Bump();
					if (const auto* tasks = F4SE::GetTaskInterface()) {
						tasks->AddTask([] {
							REX::INFO("The main menu has opened.");
							TraceLog::Mark("MAINMENU", "the main menu has opened");
							ForgetPlay();
						});
					}
				} else if (a_event.opening && a_event.menuName == RE::LoadingMenu::MENU_NAME) {
					int menuOpened = 1;
					g_unmeasured.compare_exchange_strong(menuOpened, 2);
				} else if (int loadingOpened = 2; a_event.menuName == RE::LoadingMenu::MENU_NAME && g_unmeasured.compare_exchange_strong(loadingOpened, 0)) {
					// Long after every data ready handler returned. A save load
					// closes it after kPreLoadGame cleared the flag.
					if (const auto* tasks = F4SE::GetTaskInterface()) {
						tasks->AddTask([] {
							REX::INFO("A loading screen closed with no save load or new game since the main menu opened, as after a coc from there, so NEC measures the load order again.");
							TraceLog::Mark("COC", "a loading screen closed with no save load or new game since the main menu opened");
							ArmorWear::MeasureModsAgain();
							MeasureAgain();
							WeaponWear::MeasureAgain();
						});
					}
				} else if (!a_event.opening && a_event.menuName == RE::LoadingMenu::MENU_NAME) {
					if (const auto* tasks = F4SE::GetTaskInterface()) {
						tasks->AddTask([] { Materials::ReadScrapLists(); });
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	// -------------------------------------------------------------------
	// Forgetting the last game
	// -------------------------------------------------------------------

	// What play remembers belongs to 1 game, so another save or a new game
	// starts without it, and NEC.log counts the repair boxes without gains.
	void ForgetPlay()
	{
		Jam::Unload();
		FireRate::Unload();
		InHand::Unload();
		HudParts::Weapon::Forget();
		WeaponWear::ForgetOwed();
		FoodSpoil::ForgetPlay();
		ScriptEvents::Forget();
		Papyrus::EndGame();
		Columns::SayLeftOut();
	}

	// -------------------------------------------------------------------
	// Measuring the load order again
	// -------------------------------------------------------------------

	// The recipes, crafting perks, leveled lists and characters, read again in
	// the order of Features.cpp, with the scrap lists read again from the
	// recipes Materials just found. NEC.log gets 1 line naming what moved, and
	// the time it took at debug level.
	void MeasureAgain()
	{
		const auto start = std::chrono::steady_clock::now();
		auto       moved = Materials::MeasureAgain();
		Materials::ReadScrapLists();
		for (auto& said : CraftingPerks::MeasureAgain()) {
			moved.push_back(std::move(said));
		}
		for (auto& said : Provenance::MeasureAgain()) {
			moved.push_back(std::move(said));
		}
		if (!moved.empty()) {
			REX::INFO("Since NEC last measured the load order, another mod changed {:s}.", Text::List(moved, Text::Out::kLog));
		}
		const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
		REX::DEBUG("Measured the recipes, crafting perks, leveled lists and characters again in {:d} ms.", took.count());
	}

	// -------------------------------------------------------------------
	// Watching the menus
	// -------------------------------------------------------------------

	void Begins()
	{
		g_unmeasured = 0;
	}

	// UI lasts as long as the game, so the sink is added at the first load
	// only. The first main menu opened before it, and a stat patcher such as
	// RobCo Patcher changes forms after the data loaded handler, so a coc
	// from that menu needs the measure the sink runs.
	void WatchMenus()
	{
		static bool registered = false;
		if (auto* ui = RE::UI::GetSingleton(); ui && !registered) {
			ui->RegisterSink<RE::MenuOpenCloseEvent>(new MainMenuSink());
			registered = true;
			if (ui->GetMenuOpen<RE::MainMenu>()) {
				g_unmeasured = 1;
			}
		}
	}
}
