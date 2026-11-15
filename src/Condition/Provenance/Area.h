#pragma once

#include "Condition/Provenance/Supply.h"
#include "Core/Plugin.h"

#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

// Where a place sits against the rest. A gun on a table has no leveled list, so
// the place it lies in stands in for its supply. That place is measured from
// the leveled lists its containers and characters start with, ranked as Supply
// ranks them, and placed among every container and character record measured
// the same way. Private to this folder.
namespace Provenance
{
	// What MeasureAreas finds, and the cells measured since.
	struct Areas
	{
		// The mean of every container and character record that starts with at
		// least 1 list ranked for weapons, sorted.
		std::vector<float> ladder;

		// Each cell's rank by form ID, measured at its first roll once it is
		// attached. A cell with no list is kept as UNMEASURED. The map lives
		// and dies with these Areas, which every measure, and each rise of
		// fLootAreaEffect from off, makes anew, so each starts empty.
		mutable std::mutex                               lock;
		mutable std::unordered_map<RE::TESFormID, float> ranks;
	};

	// Measures every container and character record by the weapon lists it
	// starts with and builds the ladder.
	void MeasureAreas(const Supply& a_supply, Areas& a_out);

	// Where a_cell sits on the ladder, from 0 for the poorest place to 1 for
	// the richest, or nothing when the cell has no list, its lock was busy or
	// it is not attached. Only an answer from an attached cell is kept.
	// Safe on any thread.
	std::optional<float> AreaOf(const Supply& a_supply, const Areas& a_areas, const RE::TESObjectCELL& a_cell);
}
