#include "UI/Inventory/ItemCard/Cards.h"
#include "UI/Inventory/ItemCard/ItemCard.h"

#include "Core/Settings/Settings.h"
#include "UI/Roles/Card.h"
#include "UI/Roles/Columns.h"

#include <atomic>
#include <string_view>

namespace ItemCard
{
	namespace
	{
		// Whether a list with columns opened this session. Never reset: a
		// movie's files cannot change while the game runs, and the flag holds
		// no form or movie. Set on the UI thread and read by the MCM page.
		std::atomic<bool> g_offered{ false };
	}

	void OfferColumn(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		// Only the 4 menus whose item lists show item cards are asked: a UI
		// replacer's HUD movies can carry the same settings class with no item
		// list, and asking them would show the MCM row to a player with no
		// list that has columns.
		if (!Roles::Card::For(a_file)) {
			return;
		}
		// Each of these menus loads anew at every opening, so a flip of the
		// switch shows the next time a list opens, and nothing is taken back.
		if (Roles::GiveColumn(a_movie, CND_TEXT, Settings::bCndColumn.GetValue())) {
			g_offered.store(true);
		}
	}

	bool ColumnOffered()
	{
		return g_offered.load();
	}
}
