#include "Common.h"
#include "Forms.h"
#include "Events.h"
#include "Hooks.h"
#include "Degradation.h"

RE::TESDataHandler* g_dataHandler;
RE::PlayerCharacter* g_player;

void InitializeLog()
{
	auto path = logger::log_directory();
	if (!path) {
		stl::report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= fmt::format(FMT_STRING("{:s}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::trace);
	log->flush_on(spdlog::level::trace);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%m/%d/%Y - %T] [%^%l%$] %v"s);

	logger::info(FMT_STRING("{:s} v{:s}"), Version::PROJECT, Version::NAME);
}

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
					stl::report_and_fail("Aborting - TESDataHandler::GetSingleton() failed."sv);
				}
				g_player = RE::PlayerCharacter::GetSingleton();
				if (!g_player) {
					stl::report_and_fail("Aborting - PlayerCharacter::GetSingleton() failed."sv);
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

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor");
		return false;
	}

	const auto rtv = a_f4se->RuntimeVersion();
	if (rtv < F4SE::RUNTIME_LATEST) {
		stl::report_and_fail(
			fmt::format(
				FMT_STRING("{:s} does not support runtime v{:s}."),
				Version::PROJECT,
				rtv.string()));
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	InitializeLog();

	logger::info(FMT_STRING("{:s} loaded."), Version::PROJECT);
	logger::debug("Debug logging enabled.");

	F4SE::Init(a_f4se);

	const auto messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageHandler)) {
		logger::error("Failed to register messaging listener.");
		return false;
	}

	//const auto papyrus = F4SE::GetPapyrusInterface();
	//if (!papyrus || !papyrus->Register(Papyrus::RegisterFunctions)) {
	//	logger::critical("Failed to register Papyrus functions, marking as incompatible.");
	//	return false;
	//}
	Hooks::All::Install();
	return true;
}
