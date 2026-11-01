#include "Gameplay/FoodSpoil/Food.h"

#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>
#include <optional>

// Survival rolls disease in its own script, HC_ManagerScript. A meal of food
// with 1 of its 2 food disease risk keywords runs the script's
// HandleDiseaseRiskEvent with the amount the script holds for that keyword.
// That adds to a pool, and a roll a few seconds later makes the player sick
// with the pool as its chance. Food with neither keyword, such as cooked
// food, adds no risk.
//
// After a spoiled meal NEC runs HandleDiseaseRiskEvent on the same script
// once more, so the meal adds iSpoiledSicknessMult times the fresh food's
// risk in all, and food with none of its own adds that many times the
// smallest risk any food of the load order has. The script's own rules then
// run on it: its disease rule, the most the pool takes and the roll.
//
// The keywords are found by editor ID, as Kinds.cpp finds them, and the
// amounts are read from the script at each meal. Food a mod gives other
// keywords in the script counts as food with no risk of its own.
namespace FoodSpoil
{
	namespace
	{
		// A food risk of the script: its keyword and the property with its
		// amount.
		struct Risk
		{
			const char* keyword;
			const char* amount;
		};

		// In the order the script checks them, so a food with both counts as
		// the first.
		constexpr std::array RISKS{
			Risk{ "HC_DiseaseRisk_FoodStandard", "DiseaseRiskFoodStandardAmount" },
			Risk{ "HC_DiseaseRisk_FoodHigh", "DiseaseRiskFoodHighAmount" },
		};

		// The ends iSpoiledSicknessMult counts as.
		constexpr std::int32_t LEAST_MULT = 1;
		constexpr std::int32_t MOST_MULT = 20;

		// The keywords, and which of them any food or drink of the load order
		// carries, measured at the first spoiled meal that needs them.
		struct Measured
		{
			std::array<const RE::BGSKeyword*, RISKS.size()> keywords{};
			std::array<bool, RISKS.size()>                  carried{};
		};

		std::mutex              g_lock;
		std::optional<Measured> g_measured;

		Measured Measure()
		{
			const std::scoped_lock l{ g_lock };
			if (!g_measured) {
				Measured measured;
				for (std::size_t i = 0; i < RISKS.size(); i++) {
					measured.keywords[i] = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>(RISKS[i].keyword);
				}
				for (const auto* item : g_dataHandler->GetFormArray<RE::AlchemyItem>()) {
					for (std::size_t i = 0; item && i < RISKS.size(); i++) {
						measured.carried[i] = measured.carried[i] || (measured.keywords[i] && item->HasKeyword(measured.keywords[i]));
					}
				}
				g_measured = measured;
			}
			return *g_measured;
		}

		// The script's amount of risk a_risk, or none when it holds no such
		// number, as a mod's script under the same name may not.
		std::optional<float> AmountOf(const RE::BSScript::Object& a_script, const Risk& a_risk)
		{
			const auto* amount = a_script.GetProperty(a_risk.amount);
			if (!amount || !amount->is<float>()) {
				return std::nullopt;
			}
			return RE::BSScript::get<float>(*amount);
		}

		// The first risk a_food carries, as the script picks it.
		const Risk* RiskOf(const RE::TESBoundObject& a_food, const Measured& a_measured)
		{
			if (!a_food.Is(RE::ENUM_FORM_ID::kALCH)) {
				return nullptr;
			}
			const auto& item = static_cast<const RE::AlchemyItem&>(a_food);
			for (std::size_t i = 0; i < RISKS.size(); i++) {
				if (a_measured.keywords[i] && item.HasKeyword(a_measured.keywords[i])) {
					return &RISKS[i];
				}
			}
			return nullptr;
		}

		// The smallest amount above 0 of the risks some food carries, or none.
		std::optional<float> Smallest(const RE::BSScript::Object& a_script, const Measured& a_measured)
		{
			std::optional<float> smallest;
			for (std::size_t i = 0; i < RISKS.size(); i++) {
				const auto amount = a_measured.carried[i] ? AmountOf(a_script, RISKS[i]) : std::nullopt;
				if (amount && *amount > 0.0F) {
					smallest = std::min(smallest.value_or(*amount), *amount);
				}
			}
			return smallest;
		}
	}

	void AddSickness(RE::BSScript::IVirtualMachine& a_vm, const RE::BSScript::StackFrame& a_meal, const RE::TESBoundObject& a_food)
	{
		if (!a_meal.self.is<RE::BSScript::Object>()) {
			return;
		}
		const auto script = RE::BSScript::get<RE::BSScript::Object>(a_meal.self);
		if (!script) {
			return;
		}
		const auto  measured = Measure();
		const auto  mult = std::clamp(Settings::iSpoiledSicknessMult.GetValue(), LEAST_MULT, MOST_MULT);
		const auto  name = RE::TESFullName::GetFullName(a_food);
		const auto  id = a_food.formID;
		const auto* risk = RiskOf(a_food, measured);

		// The script adds the fresh food's own risk itself, so NEC adds the
		// rest. For food with none, NEC adds it all.
		float extra = 0.0F;
		if (risk) {
			const auto own = AmountOf(*script, *risk);
			if (!own) {
				TraceLog::Line("food", "spoiled {:s} [{:08X}] eaten in Survival: its script has no {:s}, so its sickness chance stays its own",
					name, id, risk->amount);
				return;
			}
			extra = *own * static_cast<float>(mult - 1);
			TraceLog::Line("food", "spoiled {:s} [{:08X}] eaten in Survival: sickness chance {:.3f}, {:d} times the fresh food's {:.3f}", name, id,
				*own * static_cast<float>(mult), mult, *own);
		} else if (const auto smallest = Smallest(*script, measured)) {
			extra = *smallest * static_cast<float>(mult);
			TraceLog::Line("food",
				"spoiled {:s} [{:08X}] eaten in Survival: sickness chance {:.3f}, {:d} times {:.3f}, the smallest any food has, as the fresh food has none",
				name, id, extra, mult, *smallest);
		} else {
			TraceLog::Line("food", "spoiled {:s} [{:08X}] eaten in Survival: no sickness chance, as no food in the load order has one", name, id);
		}
		if (extra > 0.0F) {
			const RE::BSScript::DispatchArguments arguments{ [extra](RE::BSScrapArray<RE::BSScript::Variable>& a_args) {
				a_args = RE::BSScrapArray<RE::BSScript::Variable>{ 1 };
				a_args[0] = extra;
				return true;
			} };
			a_vm.DispatchMethodCall(script, "HandleDiseaseRiskEvent", arguments.get(), RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>{});
		}
	}

	void UnloadSickness()
	{
		const std::scoped_lock l{ g_lock };
		g_measured.reset();
	}
}
