#include "Condition/WeaponWear/WeaponWear.h"

#include "Condition/WeaponWear/Copy.h"
#include "Condition/WeaponWear/Rate.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

// An NPC's weapon wear. Their shots reach NEC in the fire hook, on whatever
// thread runs the NPC, where waiting for the NPC's inventory could freeze the
// game, see Equipped.h. So the write takes the inventory lock with a try and
// holds it as a writer for the whole write. The engine's write takes the same
// lock as a reader, which the lock allows for the thread already holding it
// as a writer, see Equipped::TryWeaponHealth.
//
// While the inventory is busy, the wear waits in a list and a task on F4SE's
// queue tries it again, once. A wear still busy then waits for the next task,
// which the next shot anybody fires or blow that lands queues, see
// SettleSoon. A task never queues itself: F4SE holds its queue lock while its
// tasks run, and a task queued from one may run in the same pass, so a task
// that kept queueing itself could spin there while the thread holding the
// inventory waits on that lock.
namespace WeaponWear
{
	namespace
	{
		// How many busy tries a wear waits through before it is dropped. A try
		// comes at most once per shot or blow, and the engine holds an
		// inventory for a moment, so only a lock that never frees gets here.
		constexpr std::uint32_t MOST_TRIES = 100;

		// 1 wear waiting for a busy inventory. The same holder, weapon and
		// source add up into 1, since the wear is the copy's rate times the
		// scale.
		struct Owed
		{
			RE::ObjectRefHandle owner;
			std::uint32_t       ownerID{ 0 };  // for the line once the holder is gone
			RE::TESObjectWEAP*  weapon{ nullptr };
			const char*         source{ nullptr };
			float               scale{ 0.0F };
			std::uint32_t       tries{ 0 };  // busy tries so far
		};

		// Taken only to add to the list or swap it out, never around a call
		// into the game.
		std::mutex        g_owedLock;
		std::vector<Owed> g_owed;

		// Counts the games ForgetOwed ended, so a task that began in the last
		// one tries and keeps nothing of it.
		std::atomic<std::uint32_t> g_game{ 0 };

		// Whether the list holds anything, read on every shot and hit without
		// the lock, see SettleSoon, and whether a task is on F4SE's queue.
		std::atomic<bool> g_anyOwed{ false };
		std::atomic<bool> g_queued{ false };

		// Adds a wear to the list. The caller holds g_owedLock. A merged entry
		// keeps the fewer tries, so a fresh wear is never dropped for the tries
		// an older one used up.
		void Owe(const Owed& a_owed)
		{
			const auto same = std::ranges::find_if(g_owed, [&a_owed](const Owed& a_other) {
				return a_other.owner == a_owed.owner && a_other.weapon == a_owed.weapon &&
				       std::string_view{ a_other.source } == a_owed.source;
			});
			if (same == g_owed.end()) {
				g_owed.push_back(a_owed);
			} else {
				same->scale += a_owed.scale;
				same->tries = std::min(same->tries, a_owed.tries);
			}
			g_anyOwed.store(true, std::memory_order_relaxed);
		}

		enum class Tried
		{
			kWorn,
			kBusy,
			kNotHeld,  // they hold no copy of the weapon
		};

		// Wears the copy now, or says the inventory is busy. a_late is the
		// header of a block for a wear that waited, opened once the lock is
		// taken or when they have no inventory. It is worded before, so the
		// holder's name is not looked up under the lock.
		Tried TryWear(RE::Actor& a_owner, RE::TESObjectWEAP& a_weapon, const char* a_source, float a_scale, std::string_view a_late)
		{
			auto* inv = a_owner.inventoryList;
			if (inv && !inv->rwLock.try_lock_write()) {
				return Tried::kBusy;
			}
			if (!a_late.empty()) {
				TraceLog::Npc::Begin("LATE", "{:s}", a_late);
			}
			if (!inv) {
				return Tried::kNotHeld;
			}
			const auto* keeper = a_owner.boolFlags.all(RE::Actor::BOOL_FLAGS::kEssential) ? &a_owner : nullptr;
			const auto  worn = WearCopy(a_owner, a_weapon, a_source, a_scale, true, keeper);
			inv->rwLock.unlock_write();
			return worn.count > 0 ? Tried::kWorn : Tried::kNotHeld;
		}

		// Tries 1 waiting wear. True once it is worn or dropped.
		bool Settled(Owed& a_owed)
		{
			const auto name = RE::TESFullName::GetFullName(*a_owed.weapon);
			const auto ref = a_owed.owner.get();
			auto*      owner = ref ? ref->As<RE::Actor>() : nullptr;
			if (!owner) {
				TraceLog::Npc::Begin("LATE", "[{:08X}] is gone, so the {:s} wear on {:s} [{:08X}] is dropped",
					a_owed.ownerID, a_owed.source, name, a_owed.weapon->formID);
				return true;
			}

			const auto late = TraceLog::IsOpen() ?
			                      std::format("{:s} with {:s} [{:08X}], {:s} wear, busy tries before: {:d}", TraceLog::Who{ owner },
									  name, a_owed.weapon->formID, a_owed.source, a_owed.tries) :
			                      std::string{};
			switch (TryWear(*owner, *a_owed.weapon, a_owed.source, a_owed.scale, late)) {
			case Tried::kWorn:
				return true;
			case Tried::kNotHeld:
				TraceLog::Npc::Line("wear", "they no longer hold it, so the wear is dropped");
				return true;
			case Tried::kBusy:
				break;
			}

			if (++a_owed.tries < MOST_TRIES) {
				return false;
			}
			REX::WARN("The inventory of {:s} stayed busy through {:d} tries, so the wear ({:s}) on {:s} [{:08X}] is dropped.",
				TraceLog::Who{ owner }, a_owed.tries, a_owed.source, name, a_owed.weapon->formID);
			return true;
		}

		// Run by F4SE, with no inventory lock held. The queued flag is cleared
		// first, so a wear owed while this runs queues another.
		void Settle()
		{
			g_queued.store(false);

			std::vector<Owed> owed;
			std::uint32_t     game = 0;
			{
				const std::scoped_lock l{ g_owedLock };
				owed.swap(g_owed);
				g_anyOwed.store(false, std::memory_order_relaxed);
				game = g_game.load();
			}

			auto left = owed.begin();
			for (auto& entry : owed) {
				if (g_game.load() != game) {
					return;
				}
				if (!Settled(entry)) {
					*left++ = entry;
				}
			}
			owed.erase(left, owed.end());
			if (owed.empty()) {
				return;
			}

			const std::scoped_lock l{ g_owedLock };
			if (g_game.load() != game) {
				return;
			}
			for (const auto& entry : owed) {
				Owe(entry);
			}
		}
	}

	bool TheirsWear()
	{
		return NpcMult() > 0.0F;
	}

	void WearTheirs(RE::Actor& a_owner, RE::TESObjectWEAP& a_weapon, const char* a_source, float a_scale)
	{
		const auto tried = TryWear(a_owner, a_weapon, a_source, a_scale, {});
		if (tried == Tried::kNotHeld) {
			TraceLog::Npc::Line("wear", "{:s} [{:08X}] is not in their hands, so no wear",
				RE::TESFullName::GetFullName(a_weapon), a_weapon.formID);
		} else if (tried == Tried::kBusy) {
			TraceLog::Npc::Line("wear", "their inventory is busy, so the wear waits for a later try");
			const auto             handle = a_owner.GetHandle();
			const std::scoped_lock l{ g_owedLock };
			Owe({ .owner = handle, .ownerID = a_owner.formID, .weapon = &a_weapon, .source = a_source, .scale = a_scale, .tries = 1 });
		}
		SettleSoon();
	}

	void SettleSoon()
	{
		if (!g_anyOwed.load(std::memory_order_relaxed) || g_queued.exchange(true)) {
			return;
		}
		if (const auto* tasks = F4SE::GetTaskInterface()) {
			tasks->AddTask(Settle);
		} else {
			g_queued.store(false);
		}
	}

	void ForgetOwed()
	{
		std::size_t dropped = 0;
		{
			const std::scoped_lock l{ g_owedLock };
			dropped = g_owed.size();
			g_owed.clear();
			g_anyOwed.store(false, std::memory_order_relaxed);
			g_game.fetch_add(1);
		}
		if (dropped > 0) {
			TraceLog::Npc::Line("wear", "{:d} NPC weapon wear still waiting is dropped with the last game", dropped);
		}
	}
}
