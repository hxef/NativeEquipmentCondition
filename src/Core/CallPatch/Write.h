#pragma once

#include "Core/CallPatch/Ledger.h"

// Writing a noted place into the game, for Ledger.cpp. Private to this folder.
namespace CallPatch
{
	// Writes the Link first, then the hook, and keeps the bytes it left, which
	// the later checks compare against.
	void WritePlace(Place& a_place);
}
