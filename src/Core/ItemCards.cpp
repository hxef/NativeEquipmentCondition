#include "Core/ItemCards.h"

namespace ItemCards
{
	void Refresh(RE::ENUM_FORM_ID a_formType)
	{
		auto* manager = RE::PipboyDataManager::GetSingleton();
		if (!manager) {
			return;
		}

		// The game holds this lock around its own rebuilds. Wear from a gun is
		// counted on an animation thread, so it matters here.
		auto& inventory = manager->inventoryData;
		inventory.LockDataGroup();
		inventory.RepopulateItemCardOnSection(a_formType);
		inventory.UnlockDataGroup();
	}
}
