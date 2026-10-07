#include "UI/Repair/VendorRepair/Payment.h"

#include "Core/Text/Text.h"
#include "Core/TraceLog.h"
#include "UI/Repair/Restore.h"
#include "UI/Repair/VendorRepair/Button.h"
#include "UI/Repair/VendorRepair/Stock.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace VendorRepair
{
	namespace
	{
		using Scaleform::GFx::Value;

		// The sound the game plays when caps change hands in a trade.
		constexpr const char* PAID_SOUND = "ITMBarter";

		// The kinds repaired since the hook that closes the screen last asked,
		// as form types, each once.
		std::vector<RE::ENUM_FORM_ID> g_cardsOwed;

		// Where the caps go: the trader's chest, or the trader where there is
		// none, as the game's own trades pay.
		[[nodiscard]] RE::TESObjectREFR* Purse(RE::BarterMenu* a_menu)
		{
			if (!a_menu) {
				return nullptr;
			}
			const auto chest = a_menu->vendorChestRef.get();
			if (chest) {
				return chest.get();
			}
			const auto trader = a_menu->vendorActor.get();
			return trader.get();
		}

		// Builds the trader's side again, from the trader and every chest
		// behind them, which also releases the copy it had.
		void Recopy(RE::BarterMenu* a_menu)
		{
			auto*      data = RE::TESDataHandler::GetSingleton();
			const auto trader = a_menu->vendorActor.get();
			if (!data || !trader) {
				return;
			}

			a_menu->SetContainerRef(data->BuildBarterContainer(trader.get()));
			a_menu->UpdateList(true);

			const auto copy = a_menu->containerRef.get();
			TraceLog::Line("menu", "{:s}'s half was built again and shows {:d} caps",
				Trader(a_menu), copy ? copy->GetGoldAmount() : 0);
		}
	}

	bool TradePending(RE::BarterMenu* a_menu)
	{
		return a_menu && !a_menu->barteredItems.empty();
	}

	void Pay(Quote a_quote, std::uint32_t a_handle, std::uint32_t a_stack)
	{
		auto*      menu = OpenBarter();
		const auto selection = Selected(menu);
		auto*      purse = Purse(menu);
		auto*      caps = Caps();
		auto*      player = RE::PlayerCharacter::GetSingleton();

		if (!menu || !player || !purse || !caps || !selection.Worn() || TradePending(menu) ||
			selection.handle != a_handle || selection.stack != a_stack ||
			a_quote.level <= selection.percent || a_quote.level > Ceiling(menu, selection)) {
			TraceLog::Line("menu", "{:s} dropped the repair, the trade or the item is gone",
				Trader(menu));
			Release();
			return;
		}

		const auto pocketHeld = player->GetGoldAmount();
		if (pocketHeld < static_cast<std::int64_t>(a_quote.price)) {
			TraceLog::Line("menu", "{:s} wanted {:d} caps for {:s} and the player had {:d}",
				Trader(menu), a_quote.price, selection.Name(), pocketHeld);
			Release();
			return;
		}

		const auto purseHeld = purse->GetGoldAmount();

		// The name is copied before the caps go and the stack is written,
		// since either can free what the selection points at: the last caps
		// leave an empty entry, and a repaired stack can merge into an
		// identical one.
		const auto name = selection.Name();

		// From the player to the trader, the same transfer the game makes when
		// a trade goes through, with the same flag that hides the message about
		// losing caps.
		{
			const RE::PlayerCharacter::ScopedInventoryChangeMessageContext quiet{ true, false };

			RE::TESObjectREFR::RemoveItemData paid{ caps, static_cast<std::int32_t>(a_quote.price) };
			paid.reason = RE::ITEM_REMOVE_REASON::kStoreContainer;
			paid.otherContainer = purse;
			player->RemoveItem(paid);
		}

		// The stack under the highlight, by its number, see Restore.h.
		RE::BGSInventoryItem::CheckStackIDFunctor find{ selection.stack };
		Restore::Write(*player, *selection.object, find, a_quote.level);

		const auto said = Text::RepairPaid(a_quote.level, a_quote.price);
		TraceLog::Line("menu", "{:s} repaired {:s} from {:d}% to {:d}%, one of a stack of {:d}, for {:d} of the {:d} caps the player had, saying \"{:s}\"",
			Trader(menu), name, selection.percent, a_quote.level, selection.count, a_quote.price, pocketHeld, said);
		TraceLog::Line("menu", "{:s} was paid into {:08X}, which held {:d} caps and now holds {:d}",
			Trader(menu), purse->GetFormID(), purseHeld, purse->GetGoldAmount());

		// Brings the screen up to date. The trader's side is built again for
		// the caps. A rebuild of the player's side only redraws the rows on the
		// screen's own list of what changed, and the caps are on it from paying
		// while an item whose count stays the same is not, so the item is added
		// here. Each rebuild ends by redrawing the lists and the caps along the
		// bottom. INVEST is checked again, as after a trade, since the player
		// may no longer have the caps for it.
		Release();
		Recopy(menu);
		menu->partialPlayerUpdateList.push_back(selection.object);
		menu->UpdateList(false);
		menu->menuObj.SetMember("canInvest"sv, Value(menu->GetInvestmentAmount() > 0));

		// The Pip-Boy's cards wait for the barter screen to close, see
		// Payment.h and ItemCards.h. The item's own kind of card.
		const auto kind = selection.object->GetFormType();
		if (std::ranges::find(g_cardsOwed, kind) == g_cardsOwed.end()) {
			g_cardsOwed.push_back(kind);
		}

		RE::UIUtils::PlayMenuSound(PAID_SOUND);
		RE::SendHUDMessage::ShowHUDMessage(said.c_str(), nullptr, true, true);
	}

	std::vector<RE::ENUM_FORM_ID> TakeCardsOwed()
	{
		return std::exchange(g_cardsOwed, {});
	}
}
