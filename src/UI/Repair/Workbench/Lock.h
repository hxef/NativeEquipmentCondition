#pragma once

#include "Core/Plugin.h"

#include "UI/Repair/Workbench/Bench.h"
#include "UI/Repair/Workbench/Cost.h"

#include <cstdint>
#include <optional>
#include <string_view>

// The repair lock: a workbench repair that takes components needs ranks of the
// crafting perk that prices it. Private to this folder.
//
// Each item asks for a rank of that perk, Gun Nut, Science!, Blacksmith or
// Armorer, see CraftingPerks.h. By its mods it asks the highest rank the
// recipes of its parts name, so a 10mm Pistol on standard parts asks none and
// one with a Suppressor asks Gun Nut 2. Every item asks the perk's last rank.
// Each rank the player lacks of what the item asks raises the condition a
// repair can start from by an equal share of iPerkLock, 100 by default, so on
// a perk of 4 ranks 1 short starts from 25 and 3 short from 75, and a mod's
// perk of 10 ranks takes 10 a rank. A press below that is turned away, all
// levels or none, since a vanilla box greys no single row. A trader and MEND
// repair whatever the lock says, and a spare too unless bLockSpares holds it
// back. Nothing is saved, so the lock reads its settings at every press.
namespace Workbench
{
	// What iRepairLock asks of every item, see Settings.h.
	enum class LockMode : std::uint8_t
	{
		kOff,
		kEveryItem,
		kByMods,
	};

	// The mode as iRepairLock stands. Any number but 0 and 1 reads as by its
	// mods, the default.
	[[nodiscard]] LockMode LockNow();

	// How the trace says a mode: "every item", "its mods" or "off".
	[[nodiscard]] std::string_view LockWord(LockMode a_mode);

	// The rank of its perk an item asks for under a_mode, never more than the
	// perk has: every rank for every item, standing.asked by its mods, 0 while
	// the lock is off.
	[[nodiscard]] std::uint32_t LockAsks(const Priced& a_priced, LockMode a_mode);

	// Why the lock turns a repair away, for the trace and the bench's words.
	// Not named Held, since Spares.h's function Held would hide it.
	struct LockHold
	{
		// The condition the item needs at its rank short.
		std::uint32_t from{ 0 };

		// The lowest rank that repairs it at its condition now.
		std::uint32_t need{ 0 };

		// The rank the item asks for, and how many ranks the player lacks.
		std::uint32_t asked{ 0 };
		std::uint32_t lacking{ 0 };
	};

	// Whether the lock turns the repair of a_selection away, priced by
	// a_priced. Nothing while the lock is off, for a repair the bench mends
	// for nothing, for one that takes only its supplies, see Supplies.h, for
	// an item no perk prices, and for a player with the rank or the
	// condition.
	[[nodiscard]] std::optional<LockHold> HeldBack(const Selection& a_selection, const Priced& a_priced);

	// Writes the lock's line in NEC.log, as the settings stand at the start.
	void SayLock();
}
