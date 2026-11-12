#include "Condition/ArmorWear/Health.h"

#include "Condition/ArmorWear/Mods.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace ArmorWear
{
	namespace
	{
		// The Health a piece with none of its own counts as, 0 while no armor
		// in the load order has any. Hits and item cards read it from other
		// threads.
		std::atomic<std::int32_t> g_median{ 0 };

		// What the log said last, so measuring again speaks only when the
		// median moved. Empty after ForgetHealth. Only the main thread
		// touches it.
		std::optional<std::int32_t> g_said;

		// The least share of its Health a copy counts for, so a mod that
		// takes all its Health away wears a piece at most 10 times as fast
		// and never breaks it in 1 blow.
		constexpr float MIN_SHARE = 0.1F;

		// -------------------------------------------------------------------
		// A copy's Health mods
		// -------------------------------------------------------------------

		// 1 Health property of a mod on the copy, with what decides its turn.
		struct Step
		{
			const RE::BGSMod::Property::Mod* property;
			std::int8_t                      priority;
			std::uint8_t                     rank;
		};

		// The copy's Health properties, in the order of its mod list. A mod
		// counts when it is for armor and not disabled on the copy. The
		// legendary mod counts too, since vanilla's Almost Unbreakable is a
		// Health mod. Health an object template sets itself is not read, as
		// Mods.cpp reads none. Empty, with nothing allocated, for a copy with
		// none.
		std::vector<Step> HealthSteps(const RE::BGSObjectInstanceExtra& a_mods)
		{
			using Property = RE::BGSMod::Property::Mod;
			using TARGET = RE::BGSMod::Property::ARMOR_TARGET;
			std::vector<Step> steps;
			for (const auto& entry : a_mods.GetIndexData()) {
				const auto* mod = entry.disabled ? nullptr : RE::TESForm::GetFormByID<RE::BGSMod::Attachment::Mod>(entry.objectID);
				if (!mod || !ForArmor(*mod)) {
					continue;
				}
				for (const auto& property : mod->GetBuffer<Property>(std::to_underlying(RE::BGSMod::Property::BLOCKIDS::kPMOD))) {
					if (static_cast<TARGET>(property.target) == TARGET::kHealth) {
						steps.push_back({ &property, mod->priority, entry.rank });
					}
				}
			}
			return steps;
		}
	}

	// -------------------------------------------------------------------
	// Measuring and asking
	// -------------------------------------------------------------------

	void MeasureHealth()
	{
		std::vector<std::int32_t> healths;
		if (g_dataHandler) {
			for (const auto* armor : g_dataHandler->GetFormArray<RE::TESObjectARMO>()) {
				const auto health = armor ? static_cast<std::int32_t>(armor->armorData.health) : 0;
				if (health > 0) {
					healths.push_back(health);
				}
			}
		}

		// The lower of the 2 in the middle for an even count. In the base game
		// and its DLC only power armor has Health, and its median is 80.
		std::int32_t median = 0;
		if (!healths.empty()) {
			const auto middle = healths.begin() + static_cast<std::ptrdiff_t>((healths.size() - 1) / 2);
			std::nth_element(healths.begin(), middle, healths.end());
			median = *middle;
		}
		g_median = median;

		if (g_said == median) {
			return;
		}
		if (median == 0) {
			REX::INFO("No armor piece in the load order has Health of its own, so armor mods that change Health change no wear.");
		} else if (!g_said) {
			REX::INFO("Armor mods that change a piece's Health change how fast it wears by the same share. A piece with no Health of its own counts as {:d} Health, the median of the {:d} armor pieces that have one.",
				median, healths.size());
		} else {
			REX::INFO("A piece with no Health of its own now counts as {:d} Health, the median of the {:d} armor pieces that have one, as a mod changed armor after game data loaded.",
				median, healths.size());
		}
		g_said = median;
	}

	void ForgetHealth()
	{
		g_median = 0;
		g_said.reset();
	}

	float HealthMult(const RE::TESObjectARMO& a_armor, const RE::ExtraDataList* a_extra)
	{
		// A copy with no mods can come back from a save with no mod list, see
		// ForEachRecipe in Materials.h.
		const auto* mods = a_extra ? a_extra->GetByType<RE::BGSObjectInstanceExtra>() : nullptr;
		if (!mods || !mods->values) {
			return 1.0F;
		}

		// A piece with no Health of its own counts as the median, so a mod
		// that adds a flat amount weighs as much on it as on a typical piece.
		const auto own = static_cast<std::int32_t>(a_armor.armorData.health);
		const auto base = own > 0 ? own : g_median.load();
		if (base <= 0) {
			return 1.0F;
		}
		auto steps = HealthSteps(*mods);
		if (steps.empty()) {
			return 1.0F;
		}

		// The game's own order, ID 2198561: by the mod's priority, lowest
		// first, and in each priority every kSet, then every kMul, then every
		// kAdd, each in the order of the mod list. Each step is the game's own
		// sum, see Property::Mod::ApplyToInt in CommonLibF4.
		std::ranges::stable_sort(steps, {}, [](const Step& a_step) { return std::pair{ a_step.priority, std::to_underlying(a_step.property->op) }; });
		auto health = base;
		for (const auto& step : steps) {
			health = step.property->ApplyToInt(health, base, step.rank);
		}
		return std::max(MIN_SHARE, static_cast<float>(health) / static_cast<float>(base));
	}
}
