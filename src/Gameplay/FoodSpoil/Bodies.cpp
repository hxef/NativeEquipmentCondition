#include "Gameplay/FoodSpoil/Food.h"

#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "Gameplay/FoodSpoil/Stamp.h"
#include "Gameplay/SpawnCondition/SpawnCondition.h"

#include <cstdint>

namespace FoodSpoil
{
	namespace
	{
		// -------------------------------------------------------------------
		// Food on the dead
		// -------------------------------------------------------------------

		// The game sends the dead event as a character or creature starts to
		// die, and again once it lies dead. For one that dies near the player
		// the 2nd comes later, so both stamp. The meat and other death items
		// arrive in between, and the door stamps them fresh as they go to a
		// body that counts as dead. So the food with no stamp here is food the
		// body carried before it died, which starts as found food.
		class DeathSink final :
			public RE::BSTEventSink<RE::TESDeathEvent>
		{
		public:
			F4_HEAP_REDEFINE_NEW(DeathSink);

		private:
			// Queued, so the body's inventory lock is taken on the engine's
			// thread with none of the engine's locks held, as the menu sweep
			// does.
			RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent& a_event, RE::BSTEventSource<RE::TESDeathEvent>*) override
			{
				if (!a_event.actorDying || !Settings::bFoodSpoils.GetValue()) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (const auto* tasks = F4SE::GetTaskInterface()) {
					tasks->AddTask([body = a_event.actorDying, dead = a_event.dead] {
						const auto today = Today();
						if (!today || !Settings::bFoodSpoils.GetValue()) {
							return;
						}
						const auto stamped = body->IsDead(true) ? StampHeld(*body, *today, true) : 0u;
						TraceLog::Line("food", "{:d} food stacks stamped on [{:08X}] at its {:s} death event", stamped, body->formID,
							dead ? "2nd" : "1st");
						if (stamped == 0) {
							return;
						}
						// A body whose inventory the game never recorded as
						// changed is filled from its record again on a load,
						// which would drop the stamps. One that got death
						// items is recorded already.
						body->AddChange(RE::CHANGE_TYPES::kRefrInventory);
					});
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	std::uint32_t StampHeld(RE::TESObjectREFR& a_holder, float a_today, bool a_died)
	{
		auto* inv = a_holder.inventoryList;
		if (!inv) {
			return 0;
		}
		std::uint32_t       stamped = 0;
		RE::BSAutoWriteLock l(inv->rwLock);
		for (auto& item : inv->data) {
			if (!item.object || !Spoils(*item.object)) {
				continue;
			}
			bool any = false;
			for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
				auto* extra = stack->extra.get();
				if (extra && !StampOf(extra) && !IsSpoiled(extra)) {
					auto start = StartOf(extra);
					if (!start && a_died && Unrolled(*extra)) {
						start = SpawnCondition::FoundFreshness(*inv, *item.object, extra, stack->count);
					}
					StampAt(*extra, *item.object, stack->count, start.value_or(1.0F), a_today);
					stamped++;
					any = true;
				}
			}
			// Food that came stamped, from the player to the dead for one,
			// keeps its older stamp, so it goes first.
			if (any) {
				OrderStacks(item);
			}
		}
		return stamped;
	}

	void WatchBodies()
	{
		static bool registered = false;
		if (registered) {
			return;
		}
		auto* source = RE::TESDeathEvent::GetEventSource();
		if (!source) {
			REX::ERROR("No death event source, so food on the dead will not spoil.");
			return;
		}
		source->RegisterSink(new DeathSink());
		registered = true;
	}
}
