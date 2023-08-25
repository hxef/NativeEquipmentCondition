#pragma once

namespace Events
{
	class All
	{
	public:
		static void Register() {
			auto* hitEventSink = RE::HitEventSource::GetSingleton();
			hitEventSink->RegisterSink(new OnPlayerMeleeHitEvent());

			auto* EquipEventsink = RE::EquipEventSource::GetSingleton();
			EquipEventsink->RegisterSink(new EquipWatcher());
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
