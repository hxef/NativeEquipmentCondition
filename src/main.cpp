#include "Common.h"
#include "Degradation.h"
#include "Events.h"
#include "Forms.h"
#include "Hooks.h"

RE::TESDataHandler* g_dataHandler;
RE::PlayerCharacter* g_player;

namespace
{
	void MessageHandler(F4SE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) {
			return;
		}

		switch (a_msg->type) {
		case F4SE::MessagingInterface::kGameDataReady:
			{
				if (static_cast<bool>(a_msg->data)) {
					g_dataHandler = RE::TESDataHandler::GetSingleton();
					if (!g_dataHandler) {
						REX::FAIL("Aborting - TESDataHandler::GetSingleton() failed."sv);
					}
					g_player = RE::PlayerCharacter::GetSingleton();
					if (!g_player) {
						REX::FAIL("Aborting - PlayerCharacter::GetSingleton() failed."sv);
					}
					Forms::Register();
					Events::All::Register();
				}
				break;
			}
		default:
			break;
		}
	}
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	REX::INFO("{} v{} loaded.", F4SE::GetPluginName(), F4SE::GetPluginVersion().string());

	const auto messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageHandler)) {
		REX::ERROR("Failed to register messaging listener.");
		return false;
	}

	Hooks::All::Install();
	return true;
}
