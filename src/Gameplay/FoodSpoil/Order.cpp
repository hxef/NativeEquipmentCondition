#include "Gameplay/FoodSpoil/FoodSpoil.h"

#include "Gameplay/FoodSpoil/Food.h"
#include "Gameplay/FoodSpoil/Stamp.h"

namespace FoodSpoil
{
	namespace
	{
		// -------------------------------------------------------------------
		// Telling the Pip-Boy
		// -------------------------------------------------------------------

		// Picks the 1 stack a_head.
		class HeadStack final :
			public RE::BGSInventoryItem::StackDataCompareFunctor
		{
		public:
			explicit HeadStack(const RE::BGSInventoryItem::Stack* a_head) noexcept :
				head(a_head)
			{}

			bool CompareData(const RE::BGSInventoryItem::Stack& a_stack) override { return &a_stack == head; }

		private:
			const RE::BGSInventoryItem::Stack* head;
		};

		// Picks the 1st stack with a stamp.
		class StampedStack final :
			public RE::BGSInventoryItem::StackDataCompareFunctor
		{
		public:
			bool CompareData(const RE::BGSInventoryItem::Stack& a_stack) override { return StampOf(a_stack.extra.get()).has_value(); }
		};

		// Writes nothing, to the whole stack.
		class WriteNothing final :
			public RE::BGSInventoryItem::StackDataWriteFunctor
		{
		public:
			WriteNothing() noexcept { shouldSplitStacks = false; }

			void WriteDataImpl(RE::TESBoundObject&, RE::BGSInventoryItem::Stack&) override {}
		};

		// The Pip-Boy listed a_item's rows when AddStack added to it, by each
		// stack's place in the chain, and a stack moved since. So the player's
		// item is announced again the way the engine announces its own
		// writes, through FindAndWriteStackData on the head with nothing to
		// write. Its merge leaves the head first, since food is never in hand.
		void Announce(const RE::BGSInventoryList& a_list, RE::BGSInventoryItem& a_item)
		{
			if (a_list.owner != RE::ObjectRefHandle{ RE::PlayerCharacter::GetPlayerHandle() }) {
				return;
			}
			HeadStack    head{ a_item.stackData.get() };
			WriteNothing nothing;
			a_item.FindAndWriteStackData(head, nothing, false, a_list.owner);
		}
	}

	void Relist(RE::TESObjectREFR& a_player, RE::TESBoundObject& a_food)
	{
		StampedStack stamped;
		WriteNothing nothing;
		a_player.FindAndWriteStackDataForInventoryItem(&a_food, stamped, nothing);
	}

	// -------------------------------------------------------------------
	// The order
	// -------------------------------------------------------------------

	bool OrderStacks(RE::BGSInventoryItem& a_item)
	{
		// The stamped stack with the earliest day. Spoiled stacks have no
		// stamp and keep their place, since they show as a row of their own.
		RE::BGSInventoryItem::Stack* oldest = nullptr;
		RE::BGSInventoryItem::Stack* before = nullptr;
		float                        first = 0.0F;
		RE::BGSInventoryItem::Stack* last = nullptr;
		for (auto* stack = a_item.stackData.get(); stack; last = stack, stack = stack->nextStack.get()) {
			const auto start = StampOf(stack->extra.get());
			if (start && (!oldest || *start < first)) {
				oldest = stack;
				before = last;
				first = *start;
			}
		}
		if (!oldest || !before) {
			return false;
		}

		// As MergeStacks moves the stack in hand to the front: the stack
		// before it points past it, it points at the old head, and it becomes
		// the head, in that order. moved keeps it alive in between.
		const RE::BSTSmartPointer<RE::BGSInventoryItem::Stack> moved{ oldest };
		before->nextStack = oldest->nextStack;
		moved->nextStack = a_item.stackData;
		a_item.stackData = moved;
		return true;
	}

	void Order(RE::BGSInventoryList& a_list, const RE::TESBoundObject& a_object)
	{
		for (auto& item : a_list.data) {
			if (item.object == &a_object) {
				if (OrderStacks(item)) {
					Announce(a_list, item);
				}
				return;
			}
		}
	}
}
