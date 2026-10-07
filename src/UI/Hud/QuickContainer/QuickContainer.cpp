#include "UI/Hud/QuickContainer/QuickContainer.h"

#include "Core/Settings.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Hud/QuickContainer/Meters.h"
#include "UI/Hud/QuickContainer/Rows.h"
#include "UI/MenuMovies.h"

namespace QuickContainer
{
	namespace
	{
		// Whether both calls were patched. The meters need both: one notes each
		// row's condition and the other receives the finished rows.
		bool g_patched = false;

		HudParts::Waiter g_waiter{ Settings::bQuickContainer, [](Scaleform::GFx::Movie& a_movie) { AddMeters(a_movie); } };
	}

	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		if (g_patched && MenuMovies::IsMovie(a_file, "HUDMenu.swf"sv)) {
			ReadNameWidth(a_movie);
			g_waiter.Watch(a_movie, "the quick container's CND");
		}
	}

	void Install()
	{
		g_patched = PatchRows();
	}
}
