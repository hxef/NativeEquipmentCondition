#include "Gameplay/FoodSpoil/Food.h"

#include "Core/ScriptEvents.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "Gameplay/FoodSpoil/Kinds.h"
#include "Gameplay/FoodSpoil/Stamp.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace FoodSpoil
{
	namespace
	{
		// -------------------------------------------------------------------
		// Turning food spoiled
		// -------------------------------------------------------------------

		// Picks a stack whose time ran out, read at the fridge speed in a
		// fridge.
		class RanOutStack final :
			public RE::BGSInventoryItem::StackDataCompareFunctor
		{
		public:
			RanOutStack(Kind a_kind, float a_today, bool a_fridge) noexcept :
				kind(a_kind),
				today(a_today),
				fridge(a_fridge)
			{}

			bool CompareData(const RE::BGSInventoryItem::Stack& a_stack) override
			{
				auto* extra = a_stack.extra.get();
				return StampOf(extra) && FreshnessIn(kind, extra, fridge, today) <= 0.0F;
			}

		private:
			Kind  kind;
			float today;
			bool  fridge;
		};

		// Turns the whole stack it is handed spoiled, never 1 piece split off.
		class SpoilStack final :
			public RE::BGSInventoryItem::StackDataWriteFunctor
		{
		public:
			SpoilStack() noexcept { shouldSplitStacks = false; }

			void WriteDataImpl(RE::TESBoundObject&, RE::BGSInventoryItem::Stack& a_stack) override
			{
				if (a_stack.extra) {
					Spoil(*a_stack.extra);
					pieces += a_stack.count;
				}
			}

			std::uint32_t pieces{ 0 };
		};

		// The foods in a_ref's inventory with stacks whose time ran out, and
		// how many such stacks each has.
		std::vector<std::pair<RE::TESBoundObject*, std::size_t>> RanOut(RE::TESObjectREFR& a_ref, float a_today, bool a_fridge)
		{
			std::vector<std::pair<RE::TESBoundObject*, std::size_t>> found;
			auto*                                                    inv = a_ref.inventoryList;
			if (!inv) {
				return found;
			}
			const RE::BSAutoReadLock l(inv->rwLock);
			for (auto& item : inv->data) {
				if (!item.object || !Spoils(*item.object)) {
					continue;
				}
				RanOutStack ranOut{ KindOf(*item.object), a_today, a_fridge };
				std::size_t stacks = 0;
				for (const auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
					if (ranOut.CompareData(*stack)) {
						stacks++;
					}
				}
				if (stacks > 0) {
					found.emplace_back(item.object, stacks);
				}
			}
			return found;
		}

		// Turns every stack in a_ref's inventory whose time ran out spoiled,
		// and returns the foods it wrote. The engine writes each stack, folds
		// the spoiled ones into 1 and, for the player, has the Pip-Boy list the
		// food again.
		std::vector<RE::TESBoundObject*> Sweep(RE::TESObjectREFR& a_ref, float a_today)
		{
			std::vector<RE::TESBoundObject*> written;
			const auto                       fridge = IsFridgeRef(&a_ref);
			for (const auto& [object, stacks] : RanOut(a_ref, a_today, fridge)) {
				RanOutStack ranOut{ KindOf(*object), a_today, fridge };
				for (std::size_t i = 0; i < stacks; i++) {
					SpoilStack spoil;
					a_ref.FindAndWriteStackDataForInventoryItem(object, ranOut, spoil);
					if (spoil.pieces == 0) {
						break;
					}
					if (i == 0) {
						written.push_back(object);
					}
					ScriptEvents::Spoiled(&a_ref, *object, spoil.pieces);
					TraceLog::Line("food", "{:s} [{:08X}] x{:d} spoiled in the inventory of [{:08X}]",
						RE::TESFullName::GetFullName(*object), object->formID, spoil.pieces, a_ref.formID);
				}
			}
			return written;
		}

		// The Pip-Boy keeps the player's rows built at all times, so a fresh
		// food's row keeps the CND it had when it was last listed. As the
		// Pip-Boy opens, each food with a stamped stack is listed again,
		// except a_listed, which the sweep wrote.
		void Refresh(RE::TESObjectREFR& a_player, const std::vector<RE::TESBoundObject*>& a_listed)
		{
			std::vector<RE::TESBoundObject*> stamped;
			if (auto* inv = a_player.inventoryList) {
				const RE::BSAutoReadLock l(inv->rwLock);
				for (auto& item : inv->data) {
					if (!item.object || !Spoils(*item.object) || std::ranges::find(a_listed, item.object) != a_listed.end()) {
						continue;
					}
					for (const auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
						if (StampOf(stack->extra.get())) {
							stamped.push_back(item.object);
							break;
						}
					}
				}
			}
			for (auto* food : stamped) {
				Relist(a_player, *food);
			}
		}

		// -------------------------------------------------------------------
		// The menus that list food
		// -------------------------------------------------------------------

		enum class Screen
		{
			kPipboy,
			kContainer,
			kBarter,
		};

		// The inventory a_screen lists beside the player's, if any.
		RE::NiPointer<RE::TESObjectREFR> OtherSide(Screen a_screen)
		{
			const auto* ui = RE::UI::GetSingleton();
			if (!ui) {
				return {};
			}
			if (a_screen == Screen::kContainer) {
				const auto menu = ui->GetMenu<RE::ContainerMenu>();
				return menu ? menu->containerRef.get() : RE::NiPointer<RE::TESObjectREFR>{};
			}
			if (a_screen == Screen::kBarter) {
				const auto menu = ui->GetMenu<RE::BarterMenu>();
				return menu ? menu->vendorChestRef.get() : RE::NiPointer<RE::TESObjectREFR>{};
			}
			return {};
		}

		// The Pip-Boy lists an item again after each write. Whether the
		// container and trade screens list again was not read, so they may
		// show a stale row until the next change.
		class MenuSink final :
			public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			F4_HEAP_REDEFINE_NEW(MenuSink);

		private:
			// Queued, since the game sends this while it works on its menus.
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (!a_event.opening || !Settings::bFoodSpoils.GetValue()) {
					return RE::BSEventNotifyControl::kContinue;
				}
				Screen screen;
				if (a_event.menuName == RE::PipboyMenu::MENU_NAME) {
					screen = Screen::kPipboy;
				} else if (a_event.menuName == RE::ContainerMenu::MENU_NAME) {
					screen = Screen::kContainer;
				} else if (a_event.menuName == RE::BarterMenu::MENU_NAME) {
					screen = Screen::kBarter;
				} else {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (const auto* tasks = F4SE::GetTaskInterface()) {
					tasks->AddTask([screen] {
						const auto today = Today();
						auto*      player = RE::PlayerCharacter::GetSingleton();
						if (!today || !player || !Settings::bFoodSpoils.GetValue()) {
							return;
						}
						const auto written = Sweep(*player, *today);
						if (screen == Screen::kPipboy) {
							Refresh(*player, written);
						}
						if (const auto other = OtherSide(screen)) {
							Sweep(*other, *today);
						}
						ScriptEvents::KickIfWaiting();
					});
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void WatchMenus()
	{
		static bool registered = false;
		auto*       ui = RE::UI::GetSingleton();
		if (registered || !ui) {
			return;
		}
		ui->RegisterSink<RE::MenuOpenCloseEvent>(new MenuSink());
		registered = true;
	}
}
