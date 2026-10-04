#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

// What a hook hands each call on to. A place leads to the game's own function,
// or to the hook of a DLL that patched it before NEC, which NEC runs on top of,
// see CallPatch.h. Each hook calls its Link with the arguments it was called
// with, never the game's function by its ID, so that DLL keeps working.
namespace CallPatch
{
	// Filled once as its place is written, before any game thread runs, and
	// never changed after, so a hook reads it with no lock.
	class LinkBase
	{
	public:
		// False at a call through a vtable that NEC found as the game left it,
		// where the hook makes the virtual call itself.
		explicit operator bool() const { return _next != 0; }

		// Marks that this place's hook ran, for a set waiting on it, see
		// Held::Runs. Only ever asked on a hook's cold path.
		void Proved() const;

		// Whether this place's set runs, 1 relaxed atomic load. A recheck
		// that cuts the place makes it false for good. A hook of a place that
		// stands alone asks it on every call and changes nothing while it is
		// false, as a hook of a Together asks its Held.
		[[nodiscard]] bool Live() const;

	protected:
		std::uintptr_t _next = 0;

	private:
		friend struct Fill;  // Ledger.cpp

		std::uint32_t _set = 0;
		std::uint16_t _mark = 0;
	};

	template <class F>
	class Link;

	// F is the hook's own type, so the call hands on exactly what the site
	// called the hook with.
	template <class R, class... A>
	class Link<R(A...)> : public LinkBase
	{
	public:
		R operator()(A... a_args) const { return reinterpret_cast<R (*)(A...)>(_next)(a_args...); }
	};

	template <class R, class... A>
	class Link<R (*)(A...)> : public Link<R(A...)>
	{};

	// One hook per site, so each site hands on to what it led to. a_make is
	// a lambda template that gives the hook of site I, an instance of a
	// template on the site's index.
	template <std::size_t N, class Make>
	[[nodiscard]] std::array<std::uintptr_t, N> PerSite(Make a_make)
	{
		return [&]<std::size_t... I>(std::index_sequence<I...>) {
			return std::array<std::uintptr_t, N>{ reinterpret_cast<std::uintptr_t>(a_make.template operator()<I>())... };
		}(std::make_index_sequence<N>{});
	}
}
