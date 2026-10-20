#pragma once

#include "Core/CallPatch/Ledger.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

// The places every open of their menu calls, and those menus' opens and
// closes. A recheck makes such a place wait for its menu instead of cutting
// it. The first call that reaches NEC's hook brings it back, and once the
// menu opens and closes with none the place is cut. A waiting place's set
// waits too, so the place counts as off for now like a held set's, see
// Losses.cpp. Private to this folder.
namespace CallPatch
{
	// A place's wait, read and written under the ledger's lock. While it
	// waits, the place's own line keeps only the DLL that cuts NEC off, for
	// the page and its part's line.
	struct MenuWait
	{
		std::string_view   what;            // the place's name, as its row noted it
		std::size_t        menu;            // which of the 3 menus calls it
		std::uint32_t      due = 0;         // the close that ends the wait, 0 when none
		bool               unread = false;  // the walk could not read the DLL's hook
		std::vector<Owner> line{};          // the walk's DLLs while it waits
	};

	// The wait of a place by its name, nullptr for any place not listed.
	// WaitingOf gives it only while the place waits. Wait starts it, due at
	// the close of the menu's next open, and Done ends it and gives the
	// walk's DLLs back. MenuUsed says that close came, read before Proving,
	// so it also sees a proof made before that close. MenuWords names the
	// menu for NEC.log.
	[[nodiscard]] MenuWait*        MenuWaitOf(const Place& a_place);
	[[nodiscard]] MenuWait*        WaitingOf(const Place& a_place);
	void                           Wait(MenuWait& a_wait, std::vector<Owner> a_line, bool a_unread);
	std::vector<Owner>             Done(MenuWait& a_wait);
	[[nodiscard]] bool             MenuUsed(const MenuWait& a_wait);
	[[nodiscard]] std::string_view MenuWords(const MenuWait& a_wait);

	// Starts counting the 3 menus' opens and closes, once. It takes the
	// game's event lock, so a recheck calls it only after letting go of the
	// ledger's.
	void CountMenus();
}
