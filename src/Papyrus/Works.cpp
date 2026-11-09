#include "Papyrus/Events.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Condition/WeaponWear/WeaponWear.h"
#include "Core/ItemCards.h"
#include "Core/ScriptEvents.h"
#include "Core/TraceLog.h"
#include "Gameplay/ArmorRating.h"
#include "Papyrus/Items.h"

#include <mutex>
#include <vector>

// The refreshes a script's write leaves for the task, kept under NEC's own
// lock and never under an inventory lock.
namespace Papyrus
{
	namespace
	{
		struct Works
		{
			std::mutex        lock;
			std::vector<Work> works;
		};

		Works& TheWorks()
		{
			static auto* works = new Works;
			return *works;
		}
	}

	// -------------------------------------------------------------------
	// Works
	// -------------------------------------------------------------------

	void Queue(const Work& a_work)
	{
		{
			auto& works = TheWorks();
			const std::scoped_lock l(works.lock);
			works.works.push_back(a_work);
		}
		ScriptEvents::Kick();
	}

	std::vector<Work> TakeWorks()
	{
		std::vector<Work> taken;
		auto&             works = TheWorks();
		const std::scoped_lock l(works.lock);
		taken.swap(works.works);
		return taken;
	}

	void PutWorksBack(std::vector<Work>&& a_works)
	{
		auto& works = TheWorks();
		const std::scoped_lock l(works.lock);
		works.works.insert(works.works.begin(), a_works.begin(), a_works.end());
	}

	void RunWork(const Work& a_work)
	{
		const auto holder = a_work.holder.get();
		auto*      actor = holder ? holder->As<RE::Actor>() : nullptr;
		auto*      base = RE::TESForm::GetFormByID(a_work.base);
		TraceLog::Papyrus::Begin("WORK", "holder={:s} base={:s} cards={:s} armor={:s} putaway={:s}", Hex(actor), Hex(base),
			a_work.cards ? "yes" : "no", a_work.armor ? "yes" : "no", a_work.putAway ? "yes" : "no");
		if (a_work.game != ScriptEvents::Game()) {
			TraceLog::Papyrus::Line("old", "from the last game");
			return;
		}
		if (!actor || !base) {
			return;
		}
		if (a_work.cards) {
			ItemCards::Refresh(base->GetFormType());
		}
		if (a_work.armor) {
			ArmorRating::Refresh(*actor);
		}

		// Only while the copy in hand still reads 0, since a repair or a
		// load may have come between.
		auto* weapon = base->As<RE::TESObjectWEAP>();
		if (a_work.putAway && weapon) {
			const auto health = Equipped::TryWeaponHealth(actor, weapon);
			if (health && *health == Condition::MIN_HEALTH) {
				WeaponWear::PutAwayNow(*actor, *weapon);
			} else {
				TraceLog::Papyrus::Line("putaway", "skipped, the weapon in hand reads {:s}", health ? ShownHealth(*health) : "busy");
			}
		}
	}
}
