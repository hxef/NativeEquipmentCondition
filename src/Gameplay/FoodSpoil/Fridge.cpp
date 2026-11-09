#include "Gameplay/FoodSpoil/Food.h"

#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "Gameplay/FoodSpoil/Stamp.h"

#include <algorithm>

namespace FoodSpoil
{
	namespace
	{
		// -------------------------------------------------------------------
		// The speed
		// -------------------------------------------------------------------

		// What the slider allows, see fFridgeSpoilSpeed. Never 0, so a store
		// always has a day to write and a take always reads it back.
		constexpr float SLOWEST = 0.01F;
		constexpr float FASTEST = 1.0F;

		float Speed()
		{
			return std::clamp(Settings::fFridgeSpoilSpeed.GetValue(), SLOWEST, FASTEST);
		}

		// -------------------------------------------------------------------
		// Who holds a stack
		// -------------------------------------------------------------------

		// Whether a stack in a_ref's inventory has a_extra. The screen may
		// list a copy of the stack, which shares its extra data, so the stacks
		// are matched by that. The engine's read lock lets the thread that
		// writes the list read it too.
		bool Holds(const RE::TESObjectREFR& a_ref, const RE::ExtraDataList* a_extra)
		{
			auto* inv = a_ref.inventoryList;
			if (!inv) {
				return false;
			}
			const RE::BSAutoReadLock l(inv->rwLock);
			for (const auto& item : inv->data) {
				for (const auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
					if (stack->extra.get() == a_extra) {
						return true;
					}
				}
			}
			return false;
		}
	}

	// -------------------------------------------------------------------
	// The stamp in a fridge
	// -------------------------------------------------------------------

	bool IsFridgeRef(const RE::TESObjectREFR* a_ref)
	{
		const auto* base = a_ref ? a_ref->GetObjectReference() : nullptr;
		return base && IsFridge(*base);
	}

	// At the slowest speed the stamp can fall up to 3000 days back, which
	// Stamp.h stores as it does any day.
	float Chill(float a_start, float a_today)
	{
		return a_today - (a_today - a_start) / Speed();
	}

	float Thaw(float a_stamp, float a_today)
	{
		return a_today - (a_today - a_stamp) * Speed();
	}

	void Restamp(RE::ExtraDataList& a_extra, const RE::TESBoundObject& a_object, std::uint32_t a_count, float a_start, bool a_into,
		float a_today)
	{
		const auto day = a_into ? Chill(a_start, a_today) : a_start;
		Stamp(a_extra, day);
		TraceLog::Line("food", "{:s} [{:08X}] x{:d} {:s} a fridge with {:.3f} days spent, stamped day {:.3f}, today is {:.3f}",
			RE::TESFullName::GetFullName(a_object), a_object.formID, a_count, a_into ? "went into" : "came out of", a_today - a_start,
			day, a_today);
	}

	// -------------------------------------------------------------------
	// The card and the quick container
	// -------------------------------------------------------------------

	// A take can hand the player a stack that shares its extra data with the
	// fridge's while the fridge still lists its own, and the door has written
	// the plain stamp on it. So a stack the player holds too reads as out of
	// the fridge.
	bool InFridge(const RE::TESObjectREFR* a_holder, const RE::ExtraDataList* a_extra)
	{
		if (!a_extra || !IsFridgeRef(a_holder) || !Holds(*a_holder, a_extra)) {
			return false;
		}
		const auto* player = RE::PlayerCharacter::GetSingleton();
		return !player || !Holds(*player, a_extra);
	}

	bool InOpenFridge(const RE::ExtraDataList* a_extra)
	{
		const auto* ui = a_extra ? RE::UI::GetSingleton() : nullptr;
		const auto  menu = ui ? ui->GetMenu<RE::ContainerMenu>() : nullptr;
		const auto  fridge = menu ? menu->containerRef.get() : RE::NiPointer<RE::TESObjectREFR>{};
		return InFridge(fridge.get(), a_extra);
	}

	bool FridgeIconOn()
	{
		return Settings::bFoodSpoils.GetValue() && Settings::bFridgeIcon.GetValue();
	}
}
