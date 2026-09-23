#include "UI/Hud/QuickContainer/QuickContainer.h"

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
	}

	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		if (!g_patched || !MenuMovies::IsMovie(a_file, "HUDMenu.swf"sv)) {
			return;
		}
		AddMeters(a_movie);
	}

	void Install()
	{
		g_patched = PatchRows();
	}
}
