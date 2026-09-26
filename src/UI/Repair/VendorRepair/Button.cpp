#include "UI/Repair/VendorRepair/Button.h"

#include "Core/Text/Text.h"
#include "Core/TraceLog.h"
#include "UI/Repair/RepairPrompt.h"
#include "UI/Repair/Restore.h"
#include "UI/Repair/VendorRepair/Quote.h"
#include "UI/Repair/VendorRepair/Stock.h"

#include <algorithm>
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

		// The sound the game plays when caps change hands in a trade, and the
		// one its menus make for a refusal.
		constexpr const char* PAID_SOUND = "ITMBarter";
		constexpr const char* REFUSED_SOUND = "UIMenuCancel";

		// Caps, where the game's list of default objects fails to name them. A
		// load order that replaces the currency says so in that list.
		constexpr RE::TESFormID CAPS_FORM = 0x0000000F;

		// Set while the question is on the screen.
		bool g_asking = false;

		// Set when a screen refuses the plugin's button. Every barter screen is
		// built the same way, so if one refuses, they all will.
		bool g_refused = false;

		// The kinds repaired since the hook that closes the screen last asked,
		// as form types, each once.
		std::vector<RE::ENUM_FORM_ID> g_cardsOwed;

		// The native function the button calls when clicked. It lives as long
		// as the plugin, the same way as the item card listener.
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

		[[nodiscard]] RE::TESBoundObject* Caps()
		{
			const auto* defaults = RE::BGSDefaultObjectManager::GetSingleton();
			auto*       named = defaults ? defaults->GetDefaultObject<RE::TESBoundObject>(
			                                   RE::DEFAULT_OBJECT::kGold) :
			                               nullptr;
			if (named) {
				return named;
			}
			auto* form = RE::TESForm::GetFormByID(CAPS_FORM);
			return form ? form->As<RE::TESBoundObject>() : nullptr;
		}

		// Puts the button on the bar, once for each barter screen.
		[[nodiscard]] bool Wire(RE::BarterMenu* a_menu)
		{
			if (!a_menu || !a_menu->uiMovie || !a_menu->menuObj.IsObject()) {
				return false;
			}

			Value already;
			if (a_menu->menuObj.GetMember(HINT_MEMBER, &already) && already.IsObject()) {
				return true;
			}
			if (g_refused) {
				return false;
			}

			// Nothing to add to yet. The list is handed over while the menu is
			// still being built, and this runs often enough to catch it.
			auto* bar = a_menu->buttonHintBar.get();
			if (!bar || !bar->sourceButtons.IsObject()) {
				return false;
			}

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

			Value hint;
			a_menu->uiMovie->CreateObject(&hint, HINT_CLASS, made.data(),
				static_cast<std::uint32_t>(made.size()));
			if (!hint.IsObject()) {
				g_refused = true;
				REX::WARN("The barter screen would not build a button hint, so no trader offers REPAIR.");
				return false;
			}

			hint.SetMember("onTextClick"sv, pressed);
			hint.SetMember("ButtonVisible"sv, Value(false));

			Value pushed;
			if (!bar->sourceButtons.Invoke("push", &pushed, &hint, 1)) {
				g_refused = true;
				REX::WARN("The barter screen would not take another button hint, so no trader offers REPAIR.");
				return false;
			}

			a_menu->menuObj.SetMember(HINT_MEMBER, hint);

			// Handing the list back is how the game says a button has changed.
			const Value again = bar->sourceButtons;
			bar->Invoke("SetButtonHintData", nullptr, &again, 1);

			Forget();
			g_asking = false;
			TraceLog::Line("menu", "{:s} offers REPAIR on the bar, keyed to {:s}",
				Trader(a_menu), HINT_EVENT);
			return true;
		}

		// Whether the barter screen should stop taking input, as the game does
		// for its own boxes: the lists stop responding and the bar hides while
		// the question is up. g_asking is only set here.
		void Hold(RE::BarterMenu* a_menu, bool a_held)
		{
			g_asking = a_held;
			if (a_menu) {
				a_menu->SetMessageBoxMode(a_held);
			}
			Refresh(a_menu);
		}

		void Release()
		{
			Hold(OpenBarter(), false);
		}

		// Pays the caps and repairs the item. Everything is checked again
		// rather than remembered, since the answer comes back through F4SE's
		// task queue a moment after, and the item has to be the one the player
		// was looking at.
		void Pay(Quote a_quote, std::uint32_t a_handle, std::uint32_t a_stack)
		{
			auto*      menu = OpenBarter();
			const auto selection = Selected(menu);
			auto*      purse = Purse(menu);
			auto*      caps = Caps();
			auto*      player = RE::PlayerCharacter::GetSingleton();

			if (!menu || !player || !purse || !caps || !selection.Worn() ||
				selection.handle != a_handle || selection.stack != a_stack ||
				a_quote.level <= selection.percent || a_quote.level > Ceiling(menu, selection.kind)) {
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

			// From the player to the trader, the same transfer the game makes
			// when a trade goes through, with the same flag that hides the
			// message about losing caps.
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

			TraceLog::Line("menu", "{:s} repaired {:s} from {:d}% to {:d}%, one of a stack of {:d}, for {:d} of the {:d} caps the player had",
				Trader(menu), selection.Name(), selection.percent, a_quote.level, selection.count, a_quote.price, pocketHeld);

			// Brings the screen up to date. A rebuild only redraws the rows on
			// the screen's own list of what changed, and the caps are on it
			// from paying while an item whose count stays the same is not, so
			// the item is added here. The rebuild ends by redrawing the lists
			// and the caps along the bottom.
			Release();
			menu->partialPlayerUpdateList.push_back(selection.object);
			menu->UpdateList(false);

			// The Pip-Boy's cards wait for the barter screen to close, see
			// Button.h and ItemCards.h. The item's own kind of card.
			const auto kind = selection.object->GetFormType();
			if (std::ranges::find(g_cardsOwed, kind) == g_cardsOwed.end()) {
				g_cardsOwed.push_back(kind);
			}

			RE::UIUtils::PlayMenuSound(PAID_SOUND);

			const auto said = Text::RepairPaid(a_quote.level, a_quote.price);
			RE::SendHUDMessage::ShowHUDMessage(said.c_str(), nullptr, true, true);
		}
	}

	void Refresh(RE::BarterMenu* a_menu)
	{
		if (!Wire(a_menu)) {
			return;
		}

		Value hint;
		if (!a_menu->menuObj.GetMember(HINT_MEMBER, &hint) || !hint.IsObject()) {
			return;
		}

		// Greyed wherever there is nothing to buy: an item past what this
		// trader repairs, or a smallest step the player cannot pay for.
		const auto selection = Selected(a_menu);
		const auto ceiling = Ceiling(a_menu, selection.kind);
		const auto shown = !g_asking && Shown(selection, ceiling);
		hint.SetMember("ButtonVisible"sv, Value(shown));
		hint.SetMember("ButtonDisabled"sv,
			Value(shown && Afforded(Quotes(selection, ceiling)).empty()));
	}

	void Press()
	{
		auto*      menu = OpenBarter();
		const auto selection = Selected(menu);
		const auto ceiling = menu ? Ceiling(menu, selection.kind) : 0U;
		if (g_asking || !menu || !Shown(selection, ceiling)) {
			return;
		}

		if (selection.percent >= ceiling) {
			const auto said = Text::RepairCeiling(ceiling);
			RE::SendHUDMessage::ShowHUDMessage(said.c_str(), nullptr, true, true);
			RE::UIUtils::PlayMenuSound(REFUSED_SOUND);
			TraceLog::Line("menu", "{:s} can take {:s} no further than {:d}%, and it is at {:d}%",
				Trader(menu), selection.Name(), ceiling, selection.percent);
			return;
		}

		TraceLog::Line("menu", "{:s} was asked to repair {:s} at {:d}%, and will go to {:d}%",
			Trader(menu), selection.Name(), selection.percent, ceiling);

		const auto quotes = Afforded(Quotes(selection, ceiling));
		if (quotes.empty()) {
			const auto said = Text::RepairUnaffordable();
			RE::SendHUDMessage::ShowHUDMessage(said.c_str(), nullptr, true, true);
			RE::UIUtils::PlayMenuSound(REFUSED_SOUND);
			TraceLog::Line("menu", "{:s} asked for more than the player has for {:s} at {:d}%",
				Trader(menu), selection.Name(), selection.percent);
			return;
		}

		std::vector<std::string> buttons;
		std::string              spelled;
		for (const auto& quote : quotes) {
			buttons.push_back(Text::RepairPrice(quote.level, quote.price));
			spelled += std::format("{:s}{:d}%:{:d}", spelled.empty() ? "" : " ", quote.level, quote.price);
		}

		TraceLog::Line("menu", "{:s} asks how far to repair {:s} at {:d}%, a stack of {:d}, worth {:d}, offering {:s}",
			Trader(menu), selection.Name(), selection.percent, selection.count, selection.worth, spelled);

		// How far this trader goes, above the question, since the buttons stop
		// at the limit and nothing else on the screen says why.
		Hold(menu, true);
		RepairPrompt::Ask(Text::RepairUpTo(ceiling), selection.Name(), selection.percent, std::move(buttons),
			[quotes, handle = selection.handle, stack = selection.stack](std::size_t a_index) {
				const auto& quote = quotes[a_index];
				TraceLog::Line("menu", "The repair was set to {:d}% for {:d} caps", quote.level, quote.price);
				Pay(quote, handle, stack);
			},
			[] {
				TraceLog::Line("menu", "The repair was called off");
				Release();
			});
	}

	bool Asking()
	{
		return g_asking;
	}

	std::vector<RE::ENUM_FORM_ID> TakeCardsOwed()
	{
		return std::exchange(g_cardsOwed, {});
	}
}
