#include "UI/MenuMovies.h"

#include "Core/Feature.h"

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

			for (const auto& feature : Features()) {
				if (feature.IsOn() && feature.OnMovieLoaded) {
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
			REX::ERROR("No Scaleform registration, so item cards keep CND under Damage and the HUD shows no CND.");
		}
	}
}
