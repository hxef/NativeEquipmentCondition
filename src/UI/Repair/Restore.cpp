#include "UI/Repair/Restore.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Gameplay/ArmorRating.h"

namespace Restore
{
	namespace
	{
		// Takes the health off, as a handover does.
		class ClearHealth final :
			public RE::BGSInventoryItem::StackDataWriteFunctor
		{
		public:
			void WriteDataImpl(RE::TESBoundObject&, RE::BGSInventoryItem::Stack& a_stack) override
			{
				if (a_stack.extra) {
					a_stack.extra->SetHealthPerc(Condition::INVALID_HEALTH);
				}
			}
		};

		void WriteWith(RE::PlayerCharacter& a_player, RE::TESBoundObject& a_object,
			RE::BGSInventoryItem::StackDataCompareFunctor& a_find, RE::BGSInventoryItem::StackDataWriteFunctor& a_write)
		{
			Equipped::WriteStack(a_player, a_object, a_find, a_write);

			if (Condition::KindOf(a_object) == Condition::Kind::kArmor) {
				ArmorRating::Refresh(a_player);
			}
		}
	}

	void Write(RE::PlayerCharacter& a_player, RE::TESBoundObject& a_object,
		RE::BGSInventoryItem::StackDataCompareFunctor& a_find, std::uint32_t a_level)
	{
		// The way the power armor station repairs.
		RE::BGSInventoryItem::SetHealthFunctor set{ Condition::FromPercent(a_level) };
		WriteWith(a_player, a_object, a_find, set);
	}

	void Clear(RE::PlayerCharacter& a_player, RE::TESBoundObject& a_object,
		RE::BGSInventoryItem::StackDataCompareFunctor& a_find)
	{
		ClearHealth clear;
		WriteWith(a_player, a_object, a_find, clear);
	}
}
