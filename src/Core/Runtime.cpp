#include "Core/Runtime.h"

#include <format>
#include <string>

namespace Runtime
{
	namespace
	{
		REL::Version g_game;
		REL::Version g_f4se;

		// 1.10.163, without the 4th number.
		std::string Short(const REL::Version& a_version)
		{
			return std::format("{:d}.{:d}.{:d}", a_version.major(), a_version.minor(), a_version.patch());
		}
	}

	void Set(const REL::Version& a_game, const REL::Version& a_f4se)
	{
		g_game = a_game;
		g_f4se = a_f4se;
	}

	bool OldGen()
	{
		return RE::IsOG();
	}

	bool Supported()
	{
		return g_game == F4SE::RUNTIME_1_11_240 || (OldGen() && g_game == F4SE::RUNTIME_1_10_163);
	}

	std::string GameVersion()
	{
		return Short(g_game);
	}

	std::string F4seVersion()
	{
		return Short(g_f4se);
	}

	std::string Line()
	{
		if (RE::GetRuntime() == RE::Runtime::kUnknown) {
			return std::format("Game version {:s} with F4SE {:s}. NEC does not know this game version, so it stays off.", GameVersion(), Short(g_f4se));
		}
		if (OldGen()) {
			return std::format("Game version {:s} (OG) with F4SE {:s}.", GameVersion(), Short(g_f4se));
		}
		return std::format("Game version {:s} with F4SE {:s}.", GameVersion(), Short(g_f4se));
	}
}
