#pragma once

#include "Core/Plugin.h"

#include "Gameplay/FoodSpoil/Kinds.h"

#include <cstdint>
#include <optional>

// The stamp: the game day a food started to spoil, kept on its inventory
// stack in the vanilla time left extra data (type 0x27), which the game itself
// only puts on equipped and placed lights. Every move, copy and save keeps it,
// and the Pip-Boy's rows ignore it, so the fresh stacks of a food show as 1
// row while stacks of different days never merge.
//
// The day is stored as minus (the day plus 10000) minus 1. A light's time
// left is never below 0, so one a mod puts on food never reads as a stamp,
// and a start day up to 10000 days before the clock started still stores
// below 0. Food in a fridge keeps a day of its own, see Food.h.
//
// A spoiled stack has no stamp and SPOILED_HEALTH, so all spoiled stacks of a
// food fold into 1 and show as a 2nd row. It is never stamped again.
//
// Found food with no stamp yet keeps its starting freshness as its health,
// above SPOILED_HEALTH and below 1, see SetStart in FoodSpoil.h. Its stamp
// takes it off. Private to this folder.
namespace FoodSpoil
{
	// The health of spoiled food. The game refuses to eat food at 0 as broken
	// gear, so spoiled food sits just above it, the same on every stack.
	inline constexpr float SPOILED_HEALTH = 0.001F;

	// The game days passed with the hour as a fraction, 2.5 at noon of the
	// 3rd day. Sleeping, waiting and fast travel move it. Nothing before the
	// game has a calendar.
	[[nodiscard]] std::optional<float> Today();

	// The day a_extra's food started to spoil, or nothing for food with no
	// stamp.
	[[nodiscard]] std::optional<float> StampOf(const RE::ExtraDataList* a_extra);

	// Whether a_extra's food has spoiled.
	[[nodiscard]] bool IsSpoiled(RE::ExtraDataList* a_extra);

	// Stamps a_extra's food as starting to spoil on a_day.
	void Stamp(RE::ExtraDataList& a_extra, float a_day);

	// Takes a_extra's stamp off and gives it the spoiled health.
	void Spoil(RE::ExtraDataList& a_extra);

	// The starting freshness a_extra's found food waits with, or nothing for
	// food that starts fresh. Nothing while Worn loot is off, so found food
	// then starts fresh as before.
	[[nodiscard]] std::optional<float> StartOf(RE::ExtraDataList* a_extra);

	// Stamps a_extra's food of a_object with a_start of its time left on
	// a_today, 1 for fresh food, and takes off the starting freshness it
	// waited with. a_count is for the trace log.
	void StampAt(RE::ExtraDataList& a_extra, const RE::TESBoundObject& a_object, std::uint32_t a_count, float a_start, float a_today);

	// How much of its time food of a_kind stamped a_start has left on a_today:
	// 1 when it starts, 0.5 halfway, 0 or less once it ran out. Raw food and
	// crops keep fRawFoodDays and cooked food fCookedFoodDays.
	[[nodiscard]] float Freshness(Kind a_kind, float a_start, float a_today);

	// The Freshness of a_extra's food of a_kind on a_today, read at the
	// fridge speed when a_fridge: 0 once spoiled, 1 before it is stamped.
	[[nodiscard]] float FreshnessIn(Kind a_kind, RE::ExtraDataList* a_extra, bool a_fridge, float a_today);
}
