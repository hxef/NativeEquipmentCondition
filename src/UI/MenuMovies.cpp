#include "UI/MenuMovies.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Feature.h"
#include "Core/Settings.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Repair/ConsoleRepair.h"

#include <Scaleform/G/GFx_MovieDef.h>

#include <algorithm>
#include <cctype>

namespace MenuMovies
{
	namespace
	{
		// F4SE calls this for every menu movie. The second argument is F4SE's
		// object for this plugin inside the movie, for plugins that give
		// ActionScript functions to call.
		bool MovieLoaded(Scaleform::GFx::Movie* a_movie, Scaleform::GFx::Value*)
		{
			const auto* definition = a_movie ? a_movie->GetMovieDef() : nullptr;
			const auto* url = definition ? definition->GetFileURL() : nullptr;
			if (!url) {
				return true;
			}

			// The URL is a path such as Interface/HUDMenu.swf, and the file
			// name says which menu it is.
			const auto path = std::string_view{ url };
			const auto slash = path.find_last_of("/\\");
			const auto file = slash == std::string_view::npos ? path : path.substr(slash + 1);

			// A HUD menu can go without NEC's delete beside another DLL, so its
			// colour targets are let go of before a new HUD or a colour change
			// reaches them, whatever the switch says.
			if (IsMovie(file, "HUDMenu.swf"sv) || IsMovie(file, "MainMenu.swf"sv)) {
				HudParts::ForgetOldTargets();
			}

			// The main and pause menus share this movie. Checked before any row
			// adds to it, so what shows there is up to date. The Settings line
			// and the summary follow only when what they say changed, by the
			// recheck or by a held set that ran again since the last summary.
			if (IsMovie(file, "MainMenu.swf"sv)) {
				if (CallPatch::Recheck("the main or pause menu opens") || CallPatch::KeepSummary(CallPatch::Summary())) {
					Settings::ReportLine();
				}
				ConsoleRepair::Settle();
			}

			for (const auto& feature : Features()) {
				// A row left to another mod adds nothing, so the HUD readouts
				// never make colour targets without the hook that frees them.
				if (feature.Runs() && feature.OnMovieLoaded) {
					feature.OnMovieLoaded(*a_movie, file);
				}
			}
			return true;
		}
	}

	bool IsMovie(std::string_view a_file, std::string_view a_name)
	{
		return std::ranges::equal(a_file, a_name, [](char a_lhs, char a_rhs) {
			return std::tolower(static_cast<unsigned char>(a_lhs)) == std::tolower(static_cast<unsigned char>(a_rhs));
		});
	}

	void Install()
	{
		const auto* scaleform = F4SE::GetScaleformInterface();
		if (!scaleform || !scaleform->Register(F4SE::GetPluginName(), MovieLoaded)) {
			REX::ERROR("Could not add NEC to the game's menus, so CND stays under Damage, the HUD shows no CND bar or meters, the Pip-Boy fades no worn out names and draws no doll bars, the bench does not fade equipped items, and NEC's settings page cannot answer.");
		}
	}
}
