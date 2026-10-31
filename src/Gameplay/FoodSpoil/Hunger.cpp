#include "Gameplay/FoodSpoil/Food.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string_view>
#include <vector>

// Survival fills hunger in its own script, HC_ManagerScript. Once the player
// eats, its ProcessSingleFoodItem reads the food's caps value with
// Form.GetGoldValue and runs every rule of its own on that number: the
// smallest meal of each hunger level, the most 1 meal fills, Ravenous Hunger
// and disease. Food that quenches thirst fills thirst from the same number.
// NEC answers that 1 read with iSpoiledHunger % of the value after the player
// ate the food spoiled, and leaves every other read vanilla.
//
// Survival raises any value from 0 up to the smallest meal of the player's
// hunger level, so spoiled food fills at least that meal even at 0. A value
// below 0 would make the player hungrier, so NEC never answers one.
namespace FoodSpoil
{
	namespace
	{
		// -------------------------------------------------------------------
		// The notes
		// -------------------------------------------------------------------

		// How long a note waits for Survival's read, which comes a few frames
		// after the meal, later while scripts lag. A note for food the script
		// never reads, such as a quest item, goes then.
		constexpr auto NOTE_LIFE = std::chrono::seconds{ 10 };

		struct Note
		{
			std::int32_t                          value;
			std::chrono::steady_clock::time_point at;
		};

		std::mutex        g_notesLock;
		std::vector<Note> g_notes;

		// Takes the oldest note for food worth a_value, and returns whether
		// there was one. The share is worked out from the value alone, so 2
		// spoiled meals of equal value are alike.
		bool TakeNote(std::int32_t a_value)
		{
			const auto             now = std::chrono::steady_clock::now();
			const std::scoped_lock l{ g_notesLock };
			std::erase_if(g_notes, [&](const Note& a_note) { return now - a_note.at > NOTE_LIFE; });
			for (auto it = g_notes.begin(); it != g_notes.end(); ++it) {
				if (it->value == a_value) {
					g_notes.erase(it);
					return true;
				}
			}
			return false;
		}

		// -------------------------------------------------------------------
		// Survival's read
		// -------------------------------------------------------------------

		using Variable = RE::BSScript::Variable;
		using StackFrame = RE::BSScript::StackFrame;
		using Dispatch_t = bool (*)(const RE::BSScript::NF_util::NativeFunctionBase*, Variable&, RE::BSScript::Internal::VirtualMachine&,
			std::uint32_t, Variable&, const StackFrame&);

		CallPatch::Link<Dispatch_t> g_link;
		REL::Relocation<Dispatch_t> _Dispatch;

		// Whether a_frame runs Survival's ProcessSingleFoodItem, by the names
		// the game holds. A mod that renames the script or the function makes
		// spoiled food fill as fresh food does.
		bool InSurvivalMeal(const StackFrame* a_frame)
		{
			const auto* function = a_frame ? a_frame->owningFunction.get() : nullptr;
			return function && function->GetName() == std::string_view{ "ProcessSingleFoodItem" } &&
			       function->GetObjectTypeName() == std::string_view{ "Hardcore:HC_ManagerScript" };
		}

		// Hands Form.GetGoldValue its arguments from a_frame, the native's own
		// frame, and its answer back in a_result. The game builds this class
		// for that 1 native alone, and a mod's natives have classes of their
		// own. The script's frame is the one below a_frame.
		bool DispatchHk(const RE::BSScript::NF_util::NativeFunctionBase* a_this, Variable& a_self,
			RE::BSScript::Internal::VirtualMachine& a_vm, std::uint32_t a_stackID, Variable& a_result, const StackFrame& a_frame)
		{
			const auto done = _Dispatch(a_this, a_self, a_vm, a_stackID, a_result, a_frame);
			if (!done || !g_link.Live() || !Settings::bFoodSpoils.GetValue() || !a_result.is<std::int32_t>() ||
				!InSurvivalMeal(a_frame.previousFrame)) {
				return done;
			}
			const auto value = RE::BSScript::get<std::int32_t>(a_result);
			if (!TakeNote(value)) {
				return done;
			}
			const auto share =
				static_cast<std::int32_t>(std::lround(static_cast<float>(value) * ShareOf(Settings::iSpoiledHunger.GetValue())));
			a_result = share;
			TraceLog::Line("food", "Survival reads {:d} of the {:d} caps value of the spoiled food eaten", share, value);
			return done;
		}
	}

	bool NoteSpoiledMeal(const RE::TESBoundObject& a_food)
	{
		if (!g_link.Live()) {
			return false;
		}
		const auto             value = static_cast<std::int32_t>(RE::TESValueForm::GetFormValue(&a_food, nullptr));
		const std::scoped_lock l{ g_notesLock };
		g_notes.push_back({ value, std::chrono::steady_clock::now() });
		return true;
	}

	void ForgetMeals()
	{
		const std::scoped_lock l{ g_notesLock };
		g_notes.clear();
	}

	void InstallHunger(Part a_group)
	{
		_Dispatch = CallPatch::PatchSlot(RE::VTABLE::BSScript__NativeFunction0_TESForm_int_[0], { 0x16, 0x16 }, DispatchHk,
			"survival hunger", a_group, true, &g_link)
		                .value_or(0);
	}
}
