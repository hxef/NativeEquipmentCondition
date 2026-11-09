#include "Core/ScriptEvents.h"

#include "Core/Runtime.h"
#include "Core/TraceLog.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <iterator>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace ScriptEvents
{
	namespace
	{
		// The most notes that wait. A note past it is dropped, the newest,
		// so the events a script already waits for keep their order.
		constexpr std::size_t MOST_NOTES = 256;

		// The byte GameVM sets as the game stops its scripts, in the loop
		// that waits for them before a save, a load and a reset, and clears
		// as they run again. Not in the fork, so its 2 places are NEC's own,
		// read in both games' code.
		struct FreezeAt
		{
			std::size_t ae;
			std::size_t og;
		};
		constexpr FreezeAt FREEZE_AT{ 0x610, 0x600 };

		// Made once and never freed, so no list is let go after the game's
		// heap has shut down at a quit.
		struct Queue
		{
			std::mutex       lock;
			std::deque<Note> notes;
		};

		Queue& TheQueue()
		{
			static auto* queue = new Queue;
			return *queue;
		}

		// Closed until game data has loaded, and from a save load's start to
		// its end.
		std::atomic<bool>          g_ready{ false };
		std::atomic<bool>          g_loading{ false };
		std::atomic<std::uint32_t> g_game{ 0 };
		std::atomic<bool>          g_queued{ false };
		std::atomic<std::uint32_t> g_overflowed{ 0 };
		void (*g_task)() = nullptr;

		void Push(Note&& a_note)
		{
			if (Closed()) {
				return;
			}
			a_note.game = g_game;
			bool full = false;
			{
				auto& queue = TheQueue();
				const std::scoped_lock l(queue.lock);
				if (queue.notes.size() < MOST_NOTES) {
					queue.notes.push_back(std::move(a_note));
				} else {
					full = true;
				}
			}
			// The site's note, and its list, go after the lock.
			if (full) {
				g_overflowed++;
				TraceLog::Papyrus::Line("dropped", "{:d} events wait, so this one is dropped", MOST_NOTES);
			}
		}

		Note NoteOf(Kind a_kind, const RE::TESObjectREFR* a_holder, const RE::TESBoundObject& a_base, RE::ExtraDataList* a_list)
		{
			Note note;
			note.kind = a_kind;
			note.holder = a_holder ? a_holder->GetFormID() : 0;
			note.base = a_base.GetFormID();
			note.list.reset(a_list);
			return note;
		}
	}

	// -------------------------------------------------------------------
	// The gate
	// -------------------------------------------------------------------

	std::optional<std::string_view> Closed()
	{
		if (!g_ready) {
			return "the game is loading every file";
		}
		if (g_loading) {
			return "a save is loading";
		}
		if (Freezing()) {
			return "the game froze its scripts";
		}
		return std::nullopt;
	}

	bool Freezing()
	{
		// Read with no lock: the thread that stops the scripts sets the byte
		// before it runs them, so a function it runs sees it.
		const auto* vm = RE::GameVM::GetSingleton();
		if (!vm) {
			return false;
		}
		const auto at = Runtime::OldGen() ? FREEZE_AT.og : FREEZE_AT.ae;
		return *(reinterpret_cast<const volatile std::uint8_t*>(vm) + at) != 0;
	}

	bool Loading()
	{
		return !g_ready || g_loading;
	}

	std::uint32_t Game()
	{
		return g_game;
	}

	void CloseForLoad()
	{
		g_loading = true;
	}

	void OpenAfterLoad()
	{
		g_loading = false;
	}

	void Bump()
	{
		g_game++;
	}

	void Forget()
	{
		g_game++;
		std::deque<Note> gone;
		{
			auto& queue = TheQueue();
			const std::scoped_lock l(queue.lock);
			gone.swap(queue.notes);
		}
	}

	void Unload()
	{
		g_ready = false;
		Forget();
	}

	void Load()
	{
		g_loading = false;
		g_ready = true;
	}

	// -------------------------------------------------------------------
	// The sites
	// -------------------------------------------------------------------

	void Changed(const RE::TESObjectREFR& a_holder, const RE::TESBoundObject& a_base, RE::ExtraDataList* a_list, float a_before,
		float a_after, Cause a_cause)
	{
		if (&a_holder != RE::PlayerCharacter::GetSingleton() || a_before == a_after) {
			return;
		}
		auto note = NoteOf(Kind::kConditionChanged, &a_holder, a_base, a_list);
		note.cause = a_cause;
		note.before = a_before;
		note.after = a_after;
		Push(std::move(note));
	}

	void Broke(const RE::TESObjectREFR* a_holder, const RE::TESBoundObject& a_base, RE::ExtraDataList* a_list, RE::ObjectRefHandle a_world)
	{
		auto note = NoteOf(Kind::kBroke, a_holder, a_base, a_holder ? a_list : nullptr);
		note.world = a_world;
		Push(std::move(note));
	}

	void Jammed(const RE::TESObjectREFR& a_actor, const RE::TESBoundObject& a_weapon)
	{
		Push(NoteOf(Kind::kJammed, &a_actor, a_weapon, nullptr));
	}

	void Spoiled(const RE::TESObjectREFR* a_holder, const RE::TESBoundObject& a_food, std::uint32_t a_count)
	{
		auto note = NoteOf(Kind::kFoodSpoiled, a_holder, a_food, nullptr);
		note.count = a_count;
		Push(std::move(note));
	}

	void Wrote(const RE::TESObjectREFR& a_holder, const RE::TESBoundObject& a_base, RE::ExtraDataList* a_list, float a_before,
		float a_after, Cause a_cause, bool a_broke)
	{
		Changed(a_holder, a_base, a_list, a_before, a_after, a_cause);
		if (a_broke) {
			Broke(&a_holder, a_base, a_list);
		}
		Kick();
	}

	void Kick()
	{
		const auto* tasks = F4SE::GetTaskInterface();
		if (!tasks || !g_task || g_queued.exchange(true)) {
			return;
		}
		// Cleared as the task starts, so a note pushed while it runs queues
		// the next one.
		tasks->AddTask([] {
			g_queued = false;
			g_task();
		});
	}

	void KickIfWaiting()
	{
		bool waiting = false;
		{
			auto& queue = TheQueue();
			const std::scoped_lock l(queue.lock);
			waiting = !queue.notes.empty();
		}
		if (waiting) {
			Kick();
		}
	}

	// -------------------------------------------------------------------
	// The task
	// -------------------------------------------------------------------

	void SetTask(void (*a_task)())
	{
		g_task = a_task;
	}

	std::vector<Note> Take()
	{
		std::vector<Note> notes;
		auto&             queue = TheQueue();
		const std::scoped_lock l(queue.lock);
		notes.reserve(queue.notes.size());
		std::move(queue.notes.begin(), queue.notes.end(), std::back_inserter(notes));
		queue.notes.clear();
		return notes;
	}

	void PutBack(std::vector<Note>&& a_notes)
	{
		auto& queue = TheQueue();
		const std::scoped_lock l(queue.lock);
		queue.notes.insert(queue.notes.begin(), std::make_move_iterator(a_notes.begin()), std::make_move_iterator(a_notes.end()));
		a_notes.clear();
	}

	std::uint32_t Overflowed()
	{
		return g_overflowed;
	}
}
