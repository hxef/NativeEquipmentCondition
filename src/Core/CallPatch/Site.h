#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

// Where NEC patches, on each game version: a call site, or a slot in a
// function table. Every place is written for 1.11.240 and 1.10.163 (OG),
// and the side of the game that runs is picked as the plugin installs. A
// place with no OG side is left off there, see Why::kNoPlace.
namespace CallPatch
{
	// Where a call sits on 1 game version: the Address Library ID of the
	// function around it plus the offset of the call instruction inside, read
	// off that version's code. The library maps a stable ID to the function's
	// address, but not offsets inside it, so every site is checked before
	// writing. jump marks a tail jump (E9) where the game has no call. OG
	// may call where 1.11.240 jumps, or the other way round.
	struct At
	{
		std::uint64_t id;
		std::uint32_t offset;
		bool          jump = false;
	};

	// 2 names for the 2 sides, so a table line cannot swap them.
	struct AE : At
	{};
	struct OG : At
	{
		// A constructor, so a table line reads OG{ id, offset } with no inner braces.
		constexpr OG(std::uint64_t a_id, std::uint32_t a_offset, bool a_jump = false, bool a_covered = false) :
			At{ a_id, a_offset, a_jump },
			covered(a_covered)
		{}

		bool covered;  // no place here, and another site of the list does its work, see NO_OG_COVERED
	};

	// No place on OG, see RE::NO_ID.
	inline constexpr OG NO_OG{ RE::NO_ID, 0 };

	// No place on OG, for a site whose work another site of the list does
	// there. PatchSites runs the rest of the list without it instead of leaving
	// the row off, and logs nothing for it.
	inline constexpr OG NO_OG_COVERED{ RE::NO_ID, 0, false, true };

	// No place on 1.11.240, for a site only OG has. Its list is patched
	// on OG only.
	inline constexpr AE NO_AE{ { RE::NO_ID, 0 } };

	// A call site, { AE{ id, offset }, NO_OG, what } until its OG side is
	// checked. what names the site in the log, short because it is on every
	// trace line.
	struct CallSite
	{
		AE          ae;
		OG          og;
		const char* what;
	};

	// The same pair for a function table slot or a vtable call's slot, written
	// in the order ae then og. Both numbers are written even where they match,
	// which shows the OG one was checked.
	inline constexpr std::size_t NO_SLOT = std::numeric_limits<std::size_t>::max();

	struct Slot
	{
		std::size_t ae;
		std::size_t og;
	};

	// The side of a site the running game has.
	[[nodiscard]] const At& Side(const CallSite& a_site);

	// Where a site's instruction sits in the running game, nothing when this
	// game version has none. A number the loaded Address Library lacks gets
	// an error line, so a slip in a table never stops the game, though a
	// missing Address Library file does.
	[[nodiscard]] std::optional<std::uintptr_t> Address(const CallSite& a_site);
}
