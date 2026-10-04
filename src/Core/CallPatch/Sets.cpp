#include "Core/CallPatch/Ledger.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>

// What a set's hooks read on every call: whether the set runs. A recheck that
// finds a place of a held set changed, while the call still reaches NEC, makes
// the set wait. Its hooks then give the game's result, so the set never runs
// half, until the changed place's own hook runs. That shows the DLL over it
// hands the call on, and the set runs again. A set that is off stays off, so
// a cut never comes back.
//
// Hooks read this with no lock. The ledger's lock covers the rest.
namespace CallPatch
{
	namespace
	{
		// A set's word, 1 relaxed load a call: 0 off, 1 runs, and above that
		// it waits on that many places less 1 to see their hook run. Each
		// place it waits on is marked by its index in the ledger.
		constexpr std::uint8_t SET_OFF = 0;
		constexpr std::uint8_t SET_LIVE = 1;

		std::array<std::atomic<std::uint8_t>, MAX_SETS> g_live{};
		std::array<std::atomic<bool>, MAX_PLACES>       g_proving{};

		// Whether a set waits after a change until its hooks run again: a
		// Together's set, unless its hooks have to run on every call. And
		// whether they have to. Set at install.
		std::array<bool, MAX_SETS> g_held{};
		std::array<bool, MAX_SETS> g_everyCall{};

		// One place fewer for a waiting set to see run. The last brings it
		// back, and an off set stays off.
		void Settle(std::uint32_t a_set)
		{
			auto& live = g_live[a_set];
			auto  now = live.load(std::memory_order_relaxed);
			while (now > SET_LIVE && !live.compare_exchange_weak(now, static_cast<std::uint8_t>(now - 1), std::memory_order_relaxed)) {}
		}
	}

	void Mark(std::uint32_t a_set, bool a_held, bool a_everyCall)
	{
		g_held[a_set] = a_held;
		g_everyCall[a_set] = a_everyCall;
	}

	void Start(std::uint32_t a_set)
	{
		g_live[a_set].store(SET_LIVE, std::memory_order_relaxed);
	}

	void Stop(std::uint32_t a_set)
	{
		g_live[a_set].store(SET_OFF, std::memory_order_relaxed);
	}

	Run RunOf(std::uint32_t a_set)
	{
		const auto now = g_live[a_set].load(std::memory_order_relaxed);
		return now == SET_OFF ? Run::kOff : now == SET_LIVE ? Run::kLive : Run::kWaiting;
	}

	bool HeldSet(std::uint32_t a_set)
	{
		return g_held[a_set];
	}

	bool EveryCall(std::uint32_t a_set)
	{
		return g_everyCall[a_set];
	}

	bool Hold(const Place& a_place)
	{
		auto& live = g_live[a_place.set];
		auto& proving = g_proving[a_place.mark];
		auto  now = live.load(std::memory_order_relaxed);
		// An off set never comes back, so a mark left from before it stopped
		// is dropped.
		if (now == SET_OFF) {
			proving.store(false, std::memory_order_relaxed);
			return false;
		}
		if (proving.load(std::memory_order_relaxed)) {
			return true;
		}
		do {
			if (now == SET_OFF || now == std::numeric_limits<std::uint8_t>::max()) {
				return false;
			}
		} while (!live.compare_exchange_weak(now, static_cast<std::uint8_t>(now + 1), std::memory_order_relaxed));
		// After the count, so a hook that takes the mark sees what it counts
		// down from.
		proving.store(true, std::memory_order_release);
		return true;
	}

	void Release(const Place& a_place)
	{
		if (g_proving[a_place.mark].exchange(false, std::memory_order_acq_rel)) {
			Settle(a_place.set);
		}
	}

	bool Proving(const Place& a_place)
	{
		return g_proving[a_place.mark].load(std::memory_order_relaxed);
	}

	// A plain load first, so a hook of a set that is off for good pays no
	// more than that on each call.
	void LinkBase::Proved() const
	{
		auto& proving = g_proving[_mark];
		if (_set != 0 && proving.load(std::memory_order_relaxed) && proving.exchange(false, std::memory_order_acq_rel)) {
			Settle(_set);
		}
	}

	bool LinkBase::Live() const
	{
		return g_live[_set].load(std::memory_order_relaxed) == SET_LIVE;
	}

	bool Held::Intact() const
	{
		return g_live[set].load(std::memory_order_relaxed) == SET_LIVE;
	}

	bool Held::Runs(const LinkBase& a_link) const
	{
		if (g_live[set].load(std::memory_order_relaxed) == SET_LIVE) {
			return true;
		}
		a_link.Proved();
		return g_live[set].load(std::memory_order_relaxed) == SET_LIVE;
	}
}
