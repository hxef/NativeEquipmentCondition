#include "Gameplay/InHand.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Core/TraceLog.h"

#include <atomic>

namespace InHand
{
	namespace
	{
		// A Reading is 8 bytes, so one atomic holds it whole.
		std::atomic<Reading> g_reading{ Reading{} };

		// Set by the first ask after the last task ran, and cleared by the
		// task it queues.
		std::atomic<bool> g_queued{ false };

		// Measures the weapon in the player's hands. False while another
		// thread holds the inventory lock, which keeps the last reading.
		bool Measure()
		{
			const auto held = Equipped::TryHeldWeapon(RE::PlayerCharacter::GetSingleton());
			if (!held) {
				TraceLog::Once("in hand", "kept the last reading, another thread had the player's inventory");
				return false;
			}

			const Reading now{ held->weapon ? held->weapon->formID : 0, held->health };
			const auto    before = g_reading.exchange(now);
			if (before.weapon == now.weapon && before.health == now.health) {
				return true;
			}
			if (held->weapon) {
				TraceLog::Line("in hand", "{:s}  health {:.6f}", TraceLog::Who{ held->weapon }, now.health);
			} else {
				TraceLog::Line("in hand", "no weapon that wears");
			}
			return true;
		}

		// Measures for the first ask after the last task and queues the task
		// that ends this reading, so the next ask measures again. F4SE runs
		// queued tasks at its next drain, up to a frame later, on a worker
		// thread during play. The task measures too when this ask found the
		// inventory busy. The try lock in TryHeldWeapon never waits, so a hook
		// can measure here.
		void Queue()
		{
			if (g_queued.exchange(true)) {
				return;
			}

			const bool  measured = Measure();
			const auto* tasks = F4SE::GetTaskInterface();
			if (!tasks) {
				g_queued = false;
				return;
			}

			tasks->AddTask([measured] {
				if (!measured) {
					Measure();
				}
				g_queued = false;
			});
		}
	}

	void Unload()
	{
		g_reading = Reading{};
	}

	Reading Now()
	{
		Queue();
		return g_reading.load();
	}

	float Health(const RE::TESForm& a_weapon)
	{
		const auto now = Now();
		return now.weapon == a_weapon.formID ? now.health : Condition::INVALID_HEALTH;
	}
}
