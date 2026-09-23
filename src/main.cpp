#include "Core/Feature.h"
#include "Core/Plugin.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "UI/MenuMovies.h"

#include <spdlog/details/os.h>

#include <ctime>
#include <format>
#include <ranges>
#include <string>
#include <string_view>

RE::TESDataHandler* g_dataHandler;

namespace
{
	// When the linker wrote this DLL, in local time, read off the DLL's own
	// header. Every build carries its own, so the log can say which one ran.
	std::string LinkTime()
	{
		const auto  base = REX::FModule::GetCurrentModule().GetBaseAddress();
		const auto* dos = reinterpret_cast<const REX::W32::IMAGE_DOS_HEADER*>(base);
		const auto* nt = reinterpret_cast<const REX::W32::IMAGE_NT_HEADERS64*>(base + dos->lfanew);
		const auto  tm = spdlog::details::os::localtime(static_cast<std::time_t>(nt->fileHeader.timeDateStamp));
		return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);
	}

	// The name of the save about to load, as F4SE hands it over with its
	// length.
	std::string_view SaveName(const F4SE::MessagingInterface::Message& a_msg)
	{
		const auto* name = static_cast<const char*>(a_msg.data);
		if (!name) {
			return "an unnamed save"sv;
		}
		const std::string_view text{ name, a_msg.dataLen };
		return text.substr(0, text.find('\0'));
	}

	void MessageHandler(F4SE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) {
			return;
		}

		if (a_msg->type == F4SE::MessagingInterface::kPreLoadGame) {
			const auto name = SaveName(*a_msg);
			REX::INFO("Loading the save {:s}.", name);
			TraceLog::Mark("LOAD", "loading the save {:s}", name);
			return;
		}

		// A save loaded or a new game begun. The Balance settings are read
		// again, so a change in NEC.ini takes a load and no restart, see
		// Settings.h.
		if (a_msg->type == F4SE::MessagingInterface::kPostLoadGame ||
			a_msg->type == F4SE::MessagingInterface::kNewGame) {
			if (a_msg->type == F4SE::MessagingInterface::kNewGame) {
				REX::INFO("A new game has begun.");
				TraceLog::Mark("NEWGAME", "a new game has begun");
			} else {
				const auto loaded = static_cast<bool>(a_msg->data);
				REX::INFO("The save {:s}.", loaded ? "has loaded" : "did not load");
				TraceLog::Mark("LOADED", "the save {:s}", loaded ? "has loaded" : "did not load");
			}
			Settings::Reload();
			Settings::Report();
			return;
		}

		if (a_msg->type != F4SE::MessagingInterface::kGameDataReady) {
			return;
		}

		// kGameDataReady says true once every file has loaded, at the start and
		// after every full reset, which is the game deleting every form and
		// loading every file again after a prompt about changed DLC, Creations
		// or a save's load order. Just before the deleting it says false. Every
		// form and the player come back at new addresses, so false is where
		// every feature releases its forms and true is where it reads them
		// again.
		const auto features = Features();
		if (!static_cast<bool>(a_msg->data)) {
			REX::INFO("The game is deleting every form to load every file again.");
			TraceLog::Mark("RESET", "the game is deleting every form to load every file again");
			// Backward, so a feature releases its forms before anything it read
			// from does.
			for (const auto& feature : std::views::reverse(features)) {
				if (feature.IsOn() && feature.Unload) {
					feature.Unload();
				}
			}
			return;
		}

		g_dataHandler = RE::TESDataHandler::GetSingleton();
		if (!g_dataHandler) {
			REX::FAIL("No TESDataHandler, aborting."sv);
		}

		TraceLog::Mark("DATA", "every file has loaded");
		for (const auto& feature : features) {
			if (feature.IsOn() && feature.Load) {
				feature.Load();
			}
		}
	}
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	// The settings first, since the log is opened at the level they name. What
	// was read is reported after F4SE::Init.
	Settings::Load();

	// The log is named after the DLL, NEC.log, and TraceLog puts the trace
	// files beside it. The trampoline is a block of executable memory near the
	// game that patched calls jump through, since a call can only reach 2
	// gigabytes and Windows can load the plugin anywhere. Each hook takes one
	// 14 byte stub, and 2 dozen hooks do not fit in the default 64 bytes.
	F4SE::Init(a_f4se, {
						  .logLevel = Settings::LogLevel(),
						  .logName = "NEC",
						  .trampoline = true,
						  .trampolineSize = 1024,
					  });

	// After F4SE::Init, which creates the main log the trace logs sit beside.
	TraceLog::Open();

	const auto built = LinkTime();
	REX::INFO("{} v{} loaded, built {:s}.", F4SE::GetPluginName(), F4SE::GetPluginVersion().string(), built);
	TraceLog::Mark("PLUGIN", "{} v{} built {:s}", F4SE::GetPluginName(), F4SE::GetPluginVersion().string(), built);
	Settings::Report();

	const auto messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageHandler)) {
		REX::ERROR("Failed to register messaging listener.");
		return false;
	}

	for (const auto& feature : Features()) {
		// A row switched off in NEC.ini is skipped in every walk of the list,
		// and the log says so once.
		if (!feature.IsOn()) {
			REX::INFO("{:s} is switched off in NEC.ini.", feature.name);
			continue;
		}
		if (feature.Install) {
			feature.Install();
		}
	}

	// The one entry point for the menu movies. F4SE takes one registration per
	// plugin, and only while the plugin loads.
	MenuMovies::Install();
	return true;
}
