#pragma once

#include "Common.h"

namespace Events
{
	namespace detail
	{
		// CommonLibF4 exposes RE::TESHitEvent::GetEventSource(), but has no equivalent
		// for equip events, so bind the game's event source singleton by address instead.
		[[nodiscard]] inline RE::BSTEventSource<RE::TESEquipEvent>* GetEquipEventSource()
		{
			static REL::Relocation<RE::BSTEventSource<RE::TESEquipEvent>*> singleton{ REL::ID(485633) };
			return singleton.get();
		}
	}

	class All
	{
	public:
		static void Register()
		{
			auto* hitEventSource = RE::TESHitEvent::GetEventSource();
			hitEventSource->RegisterSink(new OnPlayerMeleeHitEvent());

			auto* equipEventSource = detail::GetEquipEventSource();
			equipEventSource->RegisterSink(new EquipWatcher());
		}

	private:

		// Degrading player's melee weapon or player's armor.
		class OnPlayerMeleeHitEvent : public RE::BSTEventSink<RE::TESHitEvent>
		{
		public:
			F4_HEAP_REDEFINE_NEW(OnPlayerMeleeHitEvent);

		private:
			virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent& a_event, RE::BSTEventSource<RE::TESHitEvent>*) override;
		};

		// Handles when the player equips an item.
		class EquipWatcher : public RE::BSTEventSink<RE::TESEquipEvent>
		{
		public:
			F4_HEAP_REDEFINE_NEW(EquipWatcher);

		private:
			virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent& a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override;
		};
	};
}
