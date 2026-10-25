#include "Condition/ArmorWear/Skins.h"

#include <mutex>
#include <shared_mutex>
#include <unordered_set>

namespace ArmorWear
{
	namespace
	{
		using Skins = std::unordered_set<const RE::TESObjectARMO*>;

		// Readers hold the lock shared, as for the armor mods' table, since
		// hits and item cards ask from other threads.
		std::shared_mutex g_lock;
		Skins             g_skins;
	}

	void MeasureSkins()
	{
		Skins fresh;
		if (g_dataHandler) {
			for (const auto* race : g_dataHandler->GetFormArray<RE::TESRace>()) {
				if (race && race->formSkin) {
					fresh.insert(race->formSkin);
				}
			}
			for (const auto* npc : g_dataHandler->GetFormArray<RE::TESNPC>()) {
				if (npc && npc->formSkin) {
					fresh.insert(npc->formSkin);
				}
			}
		}

		// The lock is let go before the old set, now in fresh, is freed.
		const std::unique_lock l{ g_lock };
		g_skins.swap(fresh);
	}

	void UnloadSkins()
	{
		Skins old;
		const std::unique_lock l{ g_lock };
		g_skins.swap(old);
	}

	bool IsSkin(const RE::TESObjectARMO& a_armor)
	{
		const std::shared_lock l{ g_lock };
		return g_skins.contains(&a_armor);
	}
}
