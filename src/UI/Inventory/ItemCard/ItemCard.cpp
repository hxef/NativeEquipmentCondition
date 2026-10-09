#include "UI/Inventory/ItemCard/ItemCard.h"

#include "UI/Inventory/ItemCard/Cards.h"

namespace ItemCard
{
	namespace
	{
		// Whether any call took, since a card with no CND row has no row to
		// move.
		bool g_patched = false;
	}

	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		if (g_patched) {
			WatchCard(a_movie, a_file);
		}
	}

	void Install()
	{
		g_patched = PatchCards();
		PatchRates();
	}
}
