#pragma once

#include "Core/Plugin.h"

#include <string_view>

// The native targets of the sign on the barter screen. Private to this
// folder.
//
// The see through background behind the framed sign under the trader's list
// is the very one the engine paints behind the list itself. The engine paints
// it for each clip that a native target with a shaded background wraps, in
// the barter screen's list of targets, so NEC makes 1 such target for the
// sign's back clip while the sign shows.
//
// The engine draws the screen's clips only inside the box of a target that
// colours them, 1 box per target. At its own place the sign is a child of the
// trader's side and takes the side's colour, whose box grows to hold it. A
// sign the player moves leaves the side, so that box never stretches over
// the item card or the player's list, and NEC gives the clip that holds it a
// HUD colour target of its own, as vanilla's own clips have.
//
// A target must never outlive its screen: one left after the movie goes
// crashes the game at the next colour change or at its delete. So each is
// dropped as its clip comes out, as the screen closes, and in a hook on the
// barter screen's delete, the only sure end. A target whose clip left the
// display list still counts as shown and would leave a stray box, so it goes
// as soon as its clip comes out.
namespace VendorRepair::SignBack
{
	// The 2 targets the sign can hold, each at most once a screen.
	enum class Target
	{
		kBack,   // the list's background behind the panel
		kColor,  // the HUD colour of a sign moved off the trader's side
	};

	// Patches the barter screen's delete, slot 0 of its function table.
	void Install();

	// Whether the delete hook is NEC's, so a target made now is dropped
	// before its screen goes. The framed sign is built, and the sign moves
	// off its own place, only while it is.
	[[nodiscard]] bool Held();

	// Gives a_clip on a_menu a_target, once a screen, and says so for
	// a_trader. False while the delete hook is not NEC's.
	bool Make(RE::BarterMenu& a_menu, const Scaleform::GFx::Value& a_clip, std::string_view a_trader, Target a_target);

	// Drops a_menu's a_target, if it has one, saying a_how it goes. A target
	// made for another screen is left to ForgetOld.
	void Drop(RE::BarterMenu& a_menu, std::string_view a_how, Target a_target);

	// Drops both of a_menu's targets.
	void Drop(RE::BarterMenu& a_menu, std::string_view a_how);

	// Lets go of a target whose screen UI no longer lists, which a DLL over
	// the delete hook may have skipped. It stops listening for HUD colour
	// changes and is never deleted, since its destructor would touch a movie
	// that may be gone. Run a frame after a barter screen closes.
	void ForgetOld();
}
