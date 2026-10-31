#include "Gameplay/FoodSpoil/Food.h"

#include "Core/Settings.h"
#include "Core/Snapshot.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "Gameplay/FoodSpoil/Kinds.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace FoodSpoil
{
	namespace
	{
		// -------------------------------------------------------------------
		// What the load order holds
		// -------------------------------------------------------------------

		// The rads spoiled food adds on top of the food's own at
		// fSpoiledRadsMult 1, as in Fallout 76: meat and dishes made with raw
		// food 15, crops and other dishes 5.
		constexpr float MEAT_RADS = 15.0F;
		constexpr float PLANT_RADS = 5.0F;

		// The most fSpoiledRadsMult counts as.
		constexpr float MOST_RADS_MULT = 5.0F;

		struct Table
		{
			const RE::ActorValueInfo* rads = nullptr;
			// The harmful Rads effect the most food that spoils uses. Food with
			// no Rads effect of its own takes its rads through it.
			RE::EffectSetting* foodRads = nullptr;
			// Every dish made from raw food by any recipe, sorted.
			std::vector<RE::TESFormID> madeWithRaw;
		};

		// Read from other threads, since every card of spoiled food asks.
		Snapshot<Table> g_table;

		// Rads, found by editor ID. The game keeps an actor value's editor ID.
		const RE::ActorValueInfo* FindRads()
		{
			for (const auto* value : g_dataHandler->GetFormArray<RE::ActorValueInfo>()) {
				if (value && std::string_view{ value->GetFormEditorIDString().c_str() } == "Rads") {
					return value;
				}
			}
			return nullptr;
		}

		// Whether a_effect gives rads: harmful, on a_rads.
		bool GivesRads(const RE::EffectItem* a_effect, const RE::ActorValueInfo* a_rads)
		{
			const auto* setting = a_effect ? a_effect->effectSetting : nullptr;
			return setting && a_rads && setting->data.primaryAV == a_rads &&
			       setting->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kDetrimental);
		}

		// The harmful Rads effect the most food that spoils uses, the one
		// with the lowest form ID on a tie.
		RE::EffectSetting* MostUsedRads(const RE::ActorValueInfo* a_rads)
		{
			std::map<RE::TESFormID, std::pair<RE::EffectSetting*, std::size_t>> uses;
			for (const auto* food : g_dataHandler->GetFormArray<RE::AlchemyItem>()) {
				if (!food || KindOf(*food) == Kind::kNever) {
					continue;
				}
				std::vector<RE::EffectSetting*> counted;
				for (const auto* effect : food->listOfEffects) {
					if (GivesRads(effect, a_rads) && std::ranges::find(counted, effect->effectSetting) == counted.end()) {
						counted.push_back(effect->effectSetting);
						auto& use = uses[effect->effectSetting->formID];
						use.first = effect->effectSetting;
						use.second++;
					}
				}
			}
			RE::EffectSetting* most = nullptr;
			std::size_t        count = 0;
			for (const auto& [id, use] : uses) {
				if (use.second > count) {
					most = use.first;
					count = use.second;
				}
			}
			return most;
		}

		// Every dish made from raw food by any recipe.
		std::vector<RE::TESFormID> MadeWithRaw()
		{
			std::vector<RE::TESFormID> dishes;
			for (const auto* recipe : g_dataHandler->GetFormArray<RE::BGSConstructibleObject>()) {
				if (!recipe || !recipe->createdItem || !recipe->requiredItems) {
					continue;
				}
				for (const auto& part : *recipe->requiredItems) {
					if (part.first && KindOf(*part.first) == Kind::kRaw) {
						dishes.push_back(recipe->createdItem->formID);
						break;
					}
				}
			}
			std::ranges::sort(dishes);
			const auto [first, last] = std::ranges::unique(dishes);
			dishes.erase(first, last);
			return dishes;
		}

		// The rads spoiled a_food adds at fSpoiledRadsMult 1, before the game
		// scales them.
		float RadsOf(const Table& a_table, const RE::TESBoundObject& a_food)
		{
			switch (KindOf(a_food)) {
			case Kind::kRaw:
				return MEAT_RADS;
			case Kind::kCooked:
				return std::ranges::binary_search(a_table.madeWithRaw, a_food.formID) ? MEAT_RADS : PLANT_RADS;
			default:
				return PLANT_RADS;
			}
		}
	}

	void LoadRads()
	{
		if (!g_dataHandler) {
			return;
		}
		auto table = std::make_shared<Table>();
		table->rads = FindRads();
		table->foodRads = MostUsedRads(table->rads);
		table->madeWithRaw = MadeWithRaw();
		if (table->foodRads) {
			REX::INFO("Spoiled food with no rads of its own gives them through {:s} [{:08X}], the rads effect most food that spoils has.",
				RE::TESFullName::GetFullName(*table->foodRads), table->foodRads->formID);
		} else {
			REX::INFO("No food that spoils gives rads, so spoiled food adds its rads as they are.");
		}
		g_table.Publish(std::move(table));
	}

	void UnloadRads()
	{
		g_table.Publish(nullptr);
	}

	const RE::ActorValueInfo* RadsValue()
	{
		const auto table = g_table.Get();
		return table ? table->rads : nullptr;
	}

	float ExtraRads(const RE::MagicItem& a_food)
	{
		const auto table = g_table.Get();
		auto*      player = RE::PlayerCharacter::GetSingleton();
		if (!table || !player) {
			return 0.0F;
		}
		const auto rads = RadsOf(*table, a_food) * std::clamp(Settings::fSpoiledRadsMult.GetValue(), 0.0F, MOST_RADS_MULT);
		if (rads <= 0.0F) {
			return 0.0F;
		}

		const RE::EffectItem* own = nullptr;
		for (const auto* effect : a_food.listOfEffects) {
			if (GivesRads(effect, table->rads)) {
				own = effect;
				break;
			}
		}
		auto* setting = own ? own->effectSetting : table->foodRads;
		if (!setting) {
			return rads;
		}

		// The conditions the game checks before an effect does anything.
		// Lead Belly's last rank keeps food's rads off this way.
		if ((own && own->conditions && !own->conditions.IsTrue(player, player)) ||
			(setting->conditions && !setting->conditions.IsTrue(player, player))) {
			return 0.0F;
		}

		// The vanilla reckoning of what a dose of that effect does to the
		// player, which scales it by the rest.
		RE::EffectItem dose{};
		dose.data.magnitude = rads;
		dose.effectSetting = setting;
		float magnitude = 0.0F;
		float duration = 0.0F;
		RE::nsStatsMenuUtils::GetEffectDisplayInfo(const_cast<RE::MagicItem*>(&a_food), &dose, magnitude, duration);
		return std::max(magnitude, 0.0F);
	}
}
