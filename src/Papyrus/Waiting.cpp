#include "Papyrus/Waiting.h"

#include "Core/TraceLog.h"
#include "Papyrus/Items.h"
#include "Papyrus/Log.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace Papyrus::Waiting
{
	namespace
	{
		using CallResult = RE::BSScript::IFunction::CallResult;

		// The try this thread runs for a script, from Begin to End.
		struct Try
		{
			std::uint32_t stack{ 0 };
			bool          armed{ false };
			bool          locked{ false };
			std::string   holder;  // the locked inventory's holder, as the trace log names it
		};

		thread_local Try t_try;

		// How many tries found a lock, by the stack of each call still
		// waiting. Taken by script calls and as a game ends, never under an
		// inventory lock.
		std::mutex                                       g_lock;
		std::unordered_map<std::uint32_t, std::uint32_t> g_tries;
	}

	// -------------------------------------------------------------------
	// 1 try
	// -------------------------------------------------------------------

	void Begin(std::uint32_t a_stack)
	{
		t_try = { .stack = a_stack, .armed = true, .locked = false, .holder = {} };
		Log::Hold();
		TraceLog::Papyrus::Hold();
	}

	void Locked(const RE::TESObjectREFR& a_holder)
	{
		if (t_try.armed) {
			t_try.locked = true;
			t_try.holder = Hex(&a_holder);
		}
	}

	CallResult End(CallResult a_result)
	{
		const auto stack = t_try.stack;
		const bool locked = t_try.locked;
		const auto holder = std::move(t_try.holder);
		t_try = {};

		if (locked && a_result == CallResult::kCompleted) {
			bool first = false;
			{
				const std::scoped_lock l(g_lock);
				auto [at, added] = g_tries.try_emplace(stack, 0);
				first = added;
				at->second++;
			}
			// The 1st try keeps its CALL line, the first line it held.
			TraceLog::Papyrus::Release(first ? 1 : 0);
			if (first) {
				TraceLog::Papyrus::Line("wait",
					"stack={:d} {:s} is locked, so the VM runs the call again at each update, and its tries write nothing until one answers",
					stack, holder);
			}
			Log::Release(false);
			return CallResult::kFailedRetry;
		}

		std::optional<std::uint32_t> tries;
		{
			const std::scoped_lock l(g_lock);
			if (const auto at = g_tries.find(stack); at != g_tries.end()) {
				tries = at->second;
				g_tries.erase(at);
			}
		}
		TraceLog::Papyrus::Release(SIZE_MAX);
		if (tries) {
			TraceLog::Papyrus::Line("wait", "stack={:d} answered after {:d} tries that found the inventory locked", stack, *tries);
		}
		Log::Release(true);
		return a_result;
	}

	// -------------------------------------------------------------------
	// A game ending
	// -------------------------------------------------------------------

	void Forget()
	{
		const std::scoped_lock l(g_lock);
		g_tries.clear();
	}
}
