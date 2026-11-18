#include "Gameplay/FoodSpoil/Food.h"

#include "Core/ScriptEvents.h"
#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "Gameplay/FoodSpoil/Kinds.h"
#include "Gameplay/FoodSpoil/Stamp.h"
#include "Gameplay/SpawnCondition/SpawnCondition.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <thread>

namespace FoodSpoil
{
	namespace
	{
		// -------------------------------------------------------------------
		// The mark
		// -------------------------------------------------------------------

		// The marks a container carries as its own time left while SetFridge
		// has switched it against the fridge list: off on a listed one, on on
		// one on no list. Both lie between -1 and 0, where no light's time
		// left (never below 0) and no food stamp (-1 or less, see Stamp.h)
		// can be. Both are exact in a float, and the save keeps the 4 bytes as
		// they are, so they compare equal after any save and load.
		constexpr float SWITCHED_OFF = -0.5F;
		constexpr float SWITCHED_ON = -0.25F;

		// How long a read of the mark keeps trying for the list's lock.
		constexpr auto MARK_WAIT = std::chrono::milliseconds{ 50 };

		enum class Mark : std::uint8_t
		{
			kNone,
			kOff,
			kOn,
			kForeign,  // a time left that is not NEC's, read as no mark
			kUnread,   // the list's lock stayed taken for as long as the read tried
		};

		float MarkValue(Mark a_mark)
		{
			return a_mark == Mark::kOn ? SWITCHED_ON : SWITCHED_OFF;
		}

		// Whether a container with a_mark cools, its base on the list when
		// a_listed. A mark says it by itself, whatever the list says now.
		bool Cools(Mark a_mark, bool a_listed)
		{
			if (a_mark == Mark::kOn || a_mark == Mark::kOff) {
				return a_mark == Mark::kOn;
			}
			return a_listed;
		}

		// A lock taken by 1 try and let go when this goes, so every early
		// return lets go of it.
		class Tried
		{
		public:
			Tried(RE::BSReadWriteLock& a_lock, bool a_write) :
				_lock(a_lock),
				_write(a_write),
				_held(a_write ? a_lock.try_lock_write() : a_lock.try_lock_read())
			{}

			~Tried()
			{
				if (_held && _write) {
					_lock.unlock_write();
				} else if (_held) {
					_lock.unlock_read();
				}
			}

			Tried(const Tried&) = delete;
			Tried& operator=(const Tried&) = delete;

			[[nodiscard]] bool held() const noexcept { return _held; }

		private:
			RE::BSReadWriteLock& _lock;
			bool                 _write;
			bool                 _held;
		};

		// a_list's mark. The caller holds a_list's lock, as a reader or a
		// writer, across the lookup and the read: a switch frees the mark,
		// the reference's ClearData or Load frees the list's record of which
		// types it holds, and CommonLibF4's locked GetByType lets go before
		// it returns.
		Mark ReadMark(const RE::ExtraDataList& a_list)
		{
			const auto* left = static_cast<const RE::ExtraTimeLeft*>(a_list.extraData.GetByType(RE::EXTRA_DATA_TYPE::kTimeLeft));
			if (!left) {
				return Mark::kNone;
			}
			if (left->timeLeft == SWITCHED_OFF) {
				return Mark::kOff;
			}
			return left->timeLeft == SWITCHED_ON ? Mark::kOn : Mark::kForeign;
		}

		// a_ref's mark, read under its own list's read lock, taken by tries
		// for up to MARK_WAIT. It runs under the door's inventory lock, so it
		// never waits without a bound. The thread that holds the list as a
		// writer reads at once.
		Mark MarkOf(const RE::TESObjectREFR& a_ref)
		{
			const auto* list = a_ref.extraList.get();
			if (!list) {
				return Mark::kNone;
			}
			// The clock starts at the 1st miss, so a free lock costs 1 try.
			std::optional<std::chrono::steady_clock::time_point> until;
			for (;;) {
				const Tried read{ list->extraRWLock, false };
				if (read.held()) {
					return ReadMark(*list);
				}
				const auto now = std::chrono::steady_clock::now();
				if (!until) {
					until = now + MARK_WAIT;
				} else if (now >= *until) {
					return Mark::kUnread;
				}
				std::this_thread::yield();
			}
		}

		// -------------------------------------------------------------------
		// What the switch rewrites
		// -------------------------------------------------------------------

		// The stacks a switch rewrites, as the door does: stamped food that
		// is not spoiled and whose kind spoils, whatever Food spoils says.
		bool Wanted(const RE::TESBoundObject* a_object, RE::ExtraDataList* a_extra)
		{
			return a_object && a_extra && KindOf(*a_object) != Kind::kNever && StampOf(a_extra) && !IsSpoiled(a_extra);
		}

		// Whether a stack the switch rewrites in a_fridge shares its extra
		// data with a stack of a_player's, as in a store or a take under way.
		// The caller holds both locks.
		bool Shared(const RE::BGSInventoryList& a_fridge, const RE::BGSInventoryList& a_player)
		{
			for (const auto& item : a_fridge.data) {
				for (const auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
					if (!Wanted(item.object, stack->extra.get())) {
						continue;
					}
					for (const auto& mine : a_player.data) {
						for (const auto* other = mine.stackData.get(); other; other = other->nextStack.get()) {
							if (other->extra.get() == stack->extra.get()) {
								return true;
							}
						}
					}
				}
			}
			return false;
		}

		// What the switch did under its locks.
		struct Rewritten
		{
			Switched                 result{ Switched::kDone };
			const RE::TESObjectREFR* busy{ nullptr };
			std::uint32_t            stacks{ 0 };
			Mark                     mark{ Mark::kNone };
			bool                     ranOut{ false };
		};

		// Switches a_fridge, whose base is a_base and inventory a_inv, under
		// its inventory's write lock, the player's read lock and its own
		// list's write lock, each taken by 1 try. Both changes are recorded
		// before any write, so nothing is kept for this session alone.
		Rewritten Rewrite(RE::TESObjectREFR& a_fridge, const RE::TESForm& a_base, RE::BGSInventoryList* a_inv, bool a_on, float a_today)
		{
			const auto* player = RE::PlayerCharacter::GetSingleton();
			auto*       mine = player && a_inv ? player->inventoryList : nullptr;
			std::optional<Tried> fridgeLock;
			std::optional<Tried> playerLock;
			if (a_inv) {
				fridgeLock.emplace(a_inv->rwLock, true);
				if (!fridgeLock->held()) {
					return { .result = Switched::kBusy, .busy = &a_fridge };
				}
			}
			if (mine) {
				playerLock.emplace(mine->rwLock, false);
				if (!playerLock->held()) {
					return { .result = Switched::kBusy, .busy = player };
				}
			}
			auto&       list = *a_fridge.extraList;
			const Tried own{ list.extraRWLock, true };
			if (!own.held()) {
				return { .result = Switched::kBusy, .busy = &a_fridge };
			}

			// Read again under the write lock held here, since another call
			// may have come before the locks.
			const auto mark = ReadMark(list);
			const auto listed = IsFridge(a_base);
			// A container already in the asked state answers kAlready whatever
			// time left it carries. Only a change is refused over a time left
			// that is not NEC's.
			if (Cools(mark, listed) == a_on) {
				return { .result = Switched::kAlready };
			}
			if (mark == Mark::kForeign) {
				return { .result = Switched::kTaken };
			}
			// The wait line then names the player, whose inventory no other
			// thread writes but which shares a stack.
			if (a_inv && mine && Shared(*a_inv, *mine)) {
				return { .result = Switched::kBusy, .busy = player };
			}
			std::uint32_t wanted = 0;
			if (a_inv) {
				for (const auto& item : a_inv->data) {
					for (const auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
						wanted += Wanted(item.object, stack->extra.get()) ? 1 : 0;
					}
				}
			}
			// A container's inventory is recorded only with food to keep, since
			// a recorded one changes how a respawning container restocks.
			if (!a_fridge.AddChange(RE::CHANGE_TYPES::kObjectExtraItemData) ||
				(wanted > 0 && !a_fridge.AddChange(RE::CHANGE_TYPES::kRefrInventory))) {
				return { .result = Switched::kNotRecorded };
			}

			// The mark is there while any stamp is rewritten: an added or
			// changed one goes on first, a removed one comes off last.
			Rewritten done{ .mark = a_on == listed ? Mark::kNone : (a_on ? Mark::kOn : Mark::kOff) };
			if (done.mark != Mark::kNone) {
				if (auto* left = static_cast<RE::ExtraTimeLeft*>(list.extraData.GetByType(RE::EXTRA_DATA_TYPE::kTimeLeft))) {
					left->timeLeft = MarkValue(done.mark);
				} else {
					list.AddExtra(new RE::ExtraTimeLeft(MarkValue(done.mark)));
				}
			}
			if (a_inv) {
				for (const auto& item : a_inv->data) {
					for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
						auto* extra = stack->extra.get();
						if (!Wanted(item.object, extra)) {
							continue;
						}
						// The plain stamp: the stamp itself outside a fridge.
						const auto stamp = *StampOf(extra);
						const auto plain = a_on ? stamp : Thaw(stamp, a_today);
						Restamp(*extra, *item.object, stack->count, plain, a_on, a_today);
						done.ranOut = done.ranOut || (Spoils(*item.object) && Freshness(KindOf(*item.object), plain, a_today) <= 0.0F);
						done.stacks++;
					}
				}
			}
			if (done.mark == Mark::kNone) {
				list.RemoveByType(static_cast<std::uint8_t>(RE::EXTRA_DATA_TYPE::kTimeLeft));
			}
			return done;
		}
	}

	// -------------------------------------------------------------------
	// Is it a fridge now
	// -------------------------------------------------------------------

	// A mark of NEC's says the state whatever the list says. The player and
	// NPCs are no container, so they take no lock. A mark that cannot be
	// read in MARK_WAIT leaves the list's answer for that 1 call.
	bool IsFridgeRef(const RE::TESObjectREFR* a_ref)
	{
		const auto* base = a_ref ? a_ref->GetObjectReference() : nullptr;
		if (!base || !base->Is(RE::ENUM_FORM_ID::kCONT)) {
			return false;
		}
		switch (MarkOf(*a_ref)) {
		case Mark::kOff:
			return false;
		case Mark::kOn:
			return true;
		case Mark::kUnread:
			TraceLog::Line("food", "{:s} [{:08X}] its fridge mark could not be read in {:d} ms, the list answers",
				RE::TESFullName::GetFullName(*base), a_ref->formID, MARK_WAIT.count());
			[[fallthrough]];
		default:
			return IsFridge(*base);
		}
	}

	// -------------------------------------------------------------------
	// The switch
	// -------------------------------------------------------------------

	FridgeSwitch SwitchFridge(RE::TESObjectREFR& a_fridge, bool a_on)
	{
		const auto* base = a_fridge.GetObjectReference();
		if (!base || !base->Is(RE::ENUM_FORM_ID::kCONT)) {
			return { .result = Switched::kNotContainer };
		}
		if (a_fridge.IsDeleted()) {
			return { .result = Switched::kGone };
		}
		if (!SpawnCondition::CellLoaded(a_fridge) || !a_fridge.extraList) {
			return { .result = Switched::kNotLoaded };
		}
		// The mark by 1 try, so a writer on the list costs no wait. A mark
		// that could not be read goes on to the locks, which read it.
		const auto first = [&list = *a_fridge.extraList] {
			const Tried read{ list.extraRWLock, false };
			return read.held() ? ReadMark(list) : Mark::kUnread;
		}();
		if (first != Mark::kUnread && Cools(first, IsFridge(*base)) == a_on) {
			return { .result = Switched::kAlready };
		}
		const auto today = Today();
		if (!today) {
			return { .result = Switched::kNoCalendar };
		}
		const auto done = Rewrite(a_fridge, *base, a_fridge.inventoryList, a_on, *today);
		if (done.result != Switched::kDone) {
			return { .result = done.result, .busy = done.busy };
		}

		// Every lock is gone. Food whose time had already run out turns
		// spoiled as a menu would turn it, with its event.
		const auto* mark = done.mark == Mark::kOn ? "on" : (done.mark == Mark::kOff ? "off" : "none");
		TraceLog::Line("food", "{:s} [{:08X}] switched {:s} with {:d} stacks restamped, its mark now {:s}, today is {:.3f}",
			RE::TESFullName::GetFullName(*base), a_fridge.formID, a_on ? "on" : "off", done.stacks, mark, *today);
		const auto* tasks = done.ranOut ? F4SE::GetTaskInterface() : nullptr;
		if (tasks) {
			tasks->AddTask([handle = a_fridge.GetHandle()] {
				const auto ref = handle.get();
				const auto today = Today();
				if (!ref || !today || !Settings::NecOn() || !Settings::bFoodSpoils.GetValue()) {
					return;
				}
				SweepRanOut(*ref, *today);
				ScriptEvents::KickIfWaiting();
			});
		}
		return { .result = Switched::kDone, .stacks = done.stacks };
	}
}
