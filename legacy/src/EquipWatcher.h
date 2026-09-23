#pragma once

// LEGACY, not built.
//
// An event sink registered on TESEquipEvent that looked the equipped form up and
// then threw it away. It was live in the sense that it was registered and called,
// and dead in the sense that it did nothing with what it found. Carried over
// unchanged from the 2023 work until the DLL only cleanup.
//
// If equipping ever needs watching again, the sink to write is a new one. This is
// kept only so that the registration is not reinvented from scratch.

#include "Common.h"

namespace Events
{
	// Handles when the player equips an item.
	class EquipWatcher : public RE::BSTEventSink<RE::TESEquipEvent>
	{
	public:
		F4_HEAP_REDEFINE_NEW(EquipWatcher);

	private:
		RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent& a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
		{
			if (a_event.actor.get() == g_player && a_event.equipped) {
				const auto& item = RE::TESForm::GetFormByID<RE::TESObjectWEAP>(a_event.baseObject);
				(void)item;
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	// Registered alongside the hit sink:
	//
	//     auto* equipEventSource = RE::TESEquipEvent::GetEventSource();
	//     equipEventSource->RegisterSink(new EquipWatcher());
}
