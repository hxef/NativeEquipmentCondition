#pragma once

#include "Core/Plugin.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// The ordinary weapon of each damage type that enough weapons deal mostly,
// measured with the ordinary ballistic weapon, see MeasureReference.
// Private to this folder.
namespace WeaponWear
{
	// A damage type needs 5 weapons for an ordinary one of its own. With
	// fewer, a weapon would be measured against itself and wear as an
	// ordinary weapon whatever it hits for.
	inline constexpr std::size_t MIN_KIND_WEAPONS = 5;

	// A damage type with an ordinary weapon of its own.
	struct DamageKind
	{
		std::uint32_t type{ 0 };  // the damage type's form ID
		std::size_t   weapons{ 0 };
		float         median{ 0.0F };
	};
	using DamageKinds = std::vector<DamageKind>;

	// Makes a_kinds the ones wear goes by and says so in the log. a_few is how
	// many weapons deal mostly a damage type too few weapons deal. a_again for
	// a measure after the first, which logs only what moved.
	void PublishKinds(DamageKinds a_kinds, std::size_t a_few, bool a_again);

	// What the ordinary weapon of a_kind hits for, or nothing when the damage
	// type has none of its own. Read on any thread.
	std::optional<float> OwnReference(std::uint32_t a_kind);
}
