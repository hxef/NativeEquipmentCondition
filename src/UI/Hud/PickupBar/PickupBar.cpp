#include "UI/Hud/PickupBar/PickupBar.h"

#include "Core/Settings.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Hud/PickupBar/Bar.h"
#include "UI/Hud/PickupBar/Look.h"
#include "UI/MenuMovies.h"

namespace PickupBar
{
	namespace
	{
		// Whether the call that builds the prompt was patched. The bar has
		// nothing to show without it.
		bool g_patched = false;

		HudParts::Waiter g_waiter{ Settings::bPickupBar, &AddBar };
	}

	void Install()
	{
		g_patched = PatchLook();
	}

	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		if (g_patched && MenuMovies::IsMovie(a_file, "HUDMenu.swf"sv)) {
			g_waiter.Watch(a_movie, "the pickup CND bar");
		}
	}
}
