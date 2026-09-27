#include "UI/Repair/VendorRepair/VendorRepair.h"

#include "Condition/Repair.h"
#include "Core/ItemCards.h"
#include "Core/Settings.h"
#include "UI/Repair/VendorRepair/Button.h"
#include "UI/Repair/VendorRepair/Payment.h"
#include "UI/Repair/VendorRepair/Quote.h"
#include "UI/Repair/VendorRepair/Stock.h"
#include "UI/Repair/VendorRepair/Upkeep.h"

#include <cstdint>
#include <format>
#include <string>

namespace VendorRepair
{
	namespace
	{
		// What the game does before this file's hooks.
		REL::Relocation<RE::UI_MESSAGE_RESULTS (*)(RE::BarterMenu*, RE::UIMessage&)> _ProcessMessage;
		REL::Relocation<bool (*)(RE::BarterMenu*, const RE::BSFixedString&)>         _OnButtonEventRelease;
		REL::Relocation<void (*)(RE::BarterMenu*, std::int32_t, bool, std::int32_t)> _UpdateItemPickpocketInfo;
		REL::Relocation<void (*)(RE::BarterMenu*, bool)>                             _UpdateList;

		// The movie telling code where the highlight landed. It asks for the
		// pickpocket odds once per change of highlight and from nowhere else.
		void UpdateItemPickpocketInfoHk(RE::BarterMenu* a_menu, std::int32_t a_index, bool a_inContainer,
			std::int32_t a_count)
		{
			_UpdateItemPickpocketInfo(a_menu, a_index, a_inContainer, a_count);
			Highlight(a_index, a_inContainer);
			Refresh(a_menu);
		}

		// A list rebuilt, which is where the button is first put on the bar.
		// What the trader restocks with is forgotten before the game builds
		// their side again, see ForgetStock, so a highlight the rebuild sets
		// off reads it again.
		void UpdateListHk(RE::BarterMenu* a_menu, bool a_inContainer)
		{
			if (a_inContainer) {
				ForgetStock();
			}
			_UpdateList(a_menu, a_inContainer);
			Refresh(a_menu);
		}

		// The key. The barter screen answers 6 of the 17 buttons menu mode
		// names and passes the rest on. A greyed REPAIR is answered too, which
		// is how the player hears why it is greyed, and a screen with no REPAIR
		// leaves the key to the game.
		bool OnButtonEventReleaseHk(RE::BarterMenu* a_menu, const RE::BSFixedString& a_event)
		{
			if (a_event == HINT_EVENT && Offered(a_menu) && Press()) {
				return true;
			}
			return _OnButtonEventRelease(a_menu, a_event);
		}

		// A new screen forgets the last one before the game builds its lists.
		// The screen being put away: the game's own handling stops the markup
		// on every price, so the Pip-Boy is told about a repair a moment after,
		// through F4SE's task queue, once for each kind repaired.
		RE::UI_MESSAGE_RESULTS ProcessMessageHk(RE::BarterMenu* a_menu, RE::UIMessage& a_message)
		{
			if (*a_message.type == RE::UI_MESSAGE_TYPE::kShow) {
				Forget();
				ForgetButton();
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
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> menu{ RE::BarterMenu::VTABLE[0] };

		_ProcessMessage = menu.write_vfunc(0x03, ProcessMessageHk);
		_OnButtonEventRelease = menu.write_vfunc(0x0F, OnButtonEventReleaseHk);
		_UpdateItemPickpocketInfo = menu.write_vfunc(0x1F, UpdateItemPickpocketInfoHk);
		_UpdateList = menu.write_vfunc(0x20, UpdateListHk);

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

		// The stock is rolled as it enters the chest, so with spawned wear off
		// it arrives new like everything else.
		if (Settings::bSpawnCondition.GetValue()) {
			InstallUpkeep();
		}
	}
}
