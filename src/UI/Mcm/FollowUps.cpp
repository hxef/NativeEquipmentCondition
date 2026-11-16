#include "UI/Mcm/Bridge.h"

#include "Condition/CraftingPerks/CraftingPerks.h"
#include "Core/ItemCards.h"
#include "Gameplay/ArmorRating.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "Gameplay/SpawnCondition/SpawnCondition.h"

#include <array>
#include <atomic>
#include <utility>

// What the game keeps worked out from a setting, done again a frame after the
// MCM page changes it. OWES in Bridge.cpp lists which setting asks for which.
namespace Mcm
{
	namespace
	{
		// -------------------------------------------------------------------
		// What the game keeps worked out from a setting
		// -------------------------------------------------------------------

		std::atomic<bool>                                                   g_queued{ false };
		std::array<std::atomic<bool>, std::to_underlying(FollowUp::kTotal)> g_owes{};

		bool Take(FollowUp a_followUp)
		{
			return g_owes[std::to_underlying(a_followUp)].exchange(false);
		}

		// Run by F4SE a frame later, with no inventory lock held, see
		// ItemCards.h. The queued flag is cleared first, so a change made while
		// this runs queues it again and is never lost.
		void RunFollowUps()
		{
			g_queued.store(false);
			if (Take(FollowUp::kCards)) {
				ItemCards::Refresh(RE::ENUM_FORM_ID::kWEAP);
				ItemCards::Refresh(RE::ENUM_FORM_ID::kARMO);
			}
			if (Take(FollowUp::kFoodCards)) {
				ItemCards::Refresh(RE::ENUM_FORM_ID::kALCH);
			}
			if (Take(FollowUp::kResistances)) {
				if (auto* player = RE::PlayerCharacter::GetSingleton()) {
					ArmorRating::Refresh(*player);
				}
				// The game adds an NPC's resistances up again only on a change
				// of clothes, so every loaded NPC is asked as well.
				if (auto* lists = RE::ProcessLists::GetSingleton()) {
					const std::array handles{ &lists->highActorHandles, &lists->middleHighActorHandles,
						&lists->middleLowActorHandles, &lists->lowActorHandles };
					for (const auto* list : handles) {
						for (const auto& handle : *list) {
							if (const auto actor = handle.get()) {
								ArmorRating::Refresh(*actor);
							}
						}
					}
				}
			}
			if (Take(FollowUp::kCarried) && SpawnCondition::CanKeepCarried()) {
				SpawnCondition::KeepCarried();
			}
			if (Take(FollowUp::kFood) && SpawnCondition::CanKeepCarried()) {
				FoodSpoil::StampCarried();
			}
			if (Take(FollowUp::kPerks)) {
				CraftingPerks::RetellPipboy();
			}
		}
	}

	void Queue(FollowUp a_followUp)
	{
		g_owes[std::to_underlying(a_followUp)].store(true);
		if (g_queued.exchange(true)) {
			return;
		}

		const auto* tasks = F4SE::GetTaskInterface();
		if (!tasks) {
			g_queued.store(false);
			return;
		}
		tasks->AddTask(RunFollowUps);
	}
}
