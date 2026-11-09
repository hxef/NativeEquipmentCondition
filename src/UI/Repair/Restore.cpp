#include "UI/Repair/Restore.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Core/ScriptEvents.h"
#include "Gameplay/ArmorRating.h"

namespace Restore
{
	namespace
	{
		// Puts a_health on the stack, all the power armor station's repair
		// does. NEC's own copy of the game's SetHealthFunctor, so the write
		// Equipped::WriteStack hands it on to is a function of NEC's own.
		class SetHealth final :
			public RE::BGSInventoryItem::StackDataWriteFunctor
		{
		public:
			explicit SetHealth(float a_health) noexcept :
				health(a_health)
			{}

			void WriteDataImpl(RE::TESBoundObject&, RE::BGSInventoryItem::Stack& a_stack) override
			{
				if (a_stack.extra) {
					a_stack.extra->SetHealthPerc(health);
				}
			}

			float health;
		};

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
			RE::BGSInventoryItem::StackDataCompareFunctor& a_find, RE::BGSInventoryItem::StackDataWriteFunctor& a_write,
			ScriptEvents::Cause a_cause)
		{
			const auto written = Equipped::WriteStack(a_player, a_object, a_find, a_write);

			if (Condition::KindOf(a_object) == Condition::Kind::kArmor) {
				ArmorRating::Refresh(a_player);
			}
			if (!written.list) {
				return;
			}
			ScriptEvents::Wrote(a_player, a_object, written.held.get(), written.before, written.after, a_cause,
				Condition::Breaks(written.before, written.after));
		}
	}

	void Write(RE::PlayerCharacter& a_player, RE::TESBoundObject& a_object,
		RE::BGSInventoryItem::StackDataCompareFunctor& a_find, std::uint32_t a_level, ScriptEvents::Cause a_cause)
	{
		SetHealth set{ Condition::FromPercent(a_level) };
		WriteWith(a_player, a_object, a_find, set, a_cause);
	}

	void Clear(RE::PlayerCharacter& a_player, RE::TESBoundObject& a_object,
		RE::BGSInventoryItem::StackDataCompareFunctor& a_find)
	{
		ClearHealth clear;
		WriteWith(a_player, a_object, a_find, clear, ScriptEvents::Cause::kConsole);
	}
}
