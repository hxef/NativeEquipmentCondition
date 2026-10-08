#include "Core/ItemCards.h"

namespace ItemCards
{
	void Refresh(RE::ENUM_FORM_ID a_formType)
	{
		auto* manager = RE::PipboyDataManager::GetSingleton();
		if (!manager) {
			return;
		}

		// The game holds this lock around its own rebuilds. The callers run on
		// more than one thread, so it matters here.
		auto& inventory = manager->inventoryData;
		inventory.LockDataGroup();
		inventory.RepopulateItemCardOnSection(a_formType);
		inventory.UnlockDataGroup();
	}
}
