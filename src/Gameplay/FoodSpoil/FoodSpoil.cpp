#include "Gameplay/FoodSpoil/FoodSpoil.h"

#include "Core/Settings.h"
#include "Gameplay/FoodSpoil/Food.h"
#include "Gameplay/FoodSpoil/Kinds.h"
#include "Gameplay/FoodSpoil/Stamp.h"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace FoodSpoil
{
	namespace
	{
		// The player and every loaded companion, held while their food is
		// stamped.
		std::vector<RE::NiPointer<RE::Actor>> Carriers()
		{
			std::vector<RE::NiPointer<RE::Actor>> carriers;
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				carriers.emplace_back(player);
			}
			auto* lists = RE::ProcessLists::GetSingleton();
			if (!lists) {
				return carriers;
			}
			const std::array handles{ &lists->highActorHandles, &lists->middleHighActorHandles, &lists->middleLowActorHandles,
				&lists->lowActorHandles };
			for (const auto* list : handles) {
				for (const auto& handle : *list) {
					if (auto actor = handle.get(); actor && actor->IsPlayerTeammate() && !actor->IsDead(true)) {
						carriers.push_back(std::move(actor));
					}
				}
			}
			return carriers;
		}

		void StampAndSay(bool a_afterLoad)
		{
			const auto today = Today();
			if (!Settings::bFoodSpoils.GetValue() || !today) {
				return;
			}
			std::uint32_t stamped = 0;
			for (const auto& carrier : Carriers()) {
				stamped += StampHeld(*carrier, *today, false);
			}
			if (a_afterLoad) {
				REX::INFO("After the load, {:d} stacks of food you and your companions carry start to spoil.", stamped);
			} else {
				REX::INFO("Food spoiling is on. {:d} stacks of food you and your companions carry start to spoil.", stamped);
			}
		}
	}

	void Install()
	{
		InstallEat(Part::kFoodBite);
		InstallHunger(Part::kFoodHunger);
		InstallCook(Part::kFoodCook);
		InstallFertilizer(Part::kFertilizer);
	}

	void Load()
	{
		LoadKinds();
		LoadRads();
		LoadFridges();
		WatchBodies();
		WatchMenus();
		if (Settings::bFoodSpoils.GetValue()) {
			SayFertilizer();
		} else {
			REX::INFO("Food spoiling is off.");
		}
	}

	void Unload()
	{
		ForgetPlay();
		UnloadFridges();
		UnloadRads();
		UnloadKinds();
	}

	void ForgetPlay()
	{
		ForgetEaten();
		ForgetMeals();
	}

	void StampCarried()
	{
		StampAndSay(false);
	}

	void StampCarriedAfterLoad()
	{
		// A full reset is deleting every form, so it is no time to walk an
		// inventory.
		const auto* main = RE::Main::GetSingleton();
		if (!Settings::bFoodSpoils.GetValue() || (main && main->resetGame)) {
			return;
		}
		// A task, so the companions are read on the engine's thread.
		if (const auto* tasks = F4SE::GetTaskInterface()) {
			tasks->AddTask([] { StampAndSay(true); });
		}
	}

	bool Spoils(const RE::TESBoundObject& a_object)
	{
		return Settings::bFoodSpoils.GetValue() && KindOf(a_object) != Kind::kNever;
	}

	bool SpoiledStack(const RE::TESBoundObject& a_object, RE::ExtraDataList* a_extra)
	{
		const auto freshness = FreshnessOf(a_object, a_extra);
		return freshness && *freshness <= 0.0F;
	}
}
