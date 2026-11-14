#include "Condition/Provenance/Area.h"

#include "Condition/Provenance/Provenance.h"
#include "Condition/Provenance/Rank.h"
#include "Condition/Provenance/Supply.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <span>
#include <string_view>
#include <utility>

namespace Provenance
{
	namespace
	{
		// -------------------------------------------------------------------
		// The ladder
		// -------------------------------------------------------------------

		// The smallest difference between 2 means worth ranking on. Means of
		// the same lists taken in another order differ in the last float bits,
		// and equal has to mean equal.
		constexpr double AREA_STEP = 0.0001;

		// The weapon lists counted so far, added up.
		struct Tally
		{
			double      sum{ 0.0 };
			std::size_t lists{ 0 };

			[[nodiscard]] float Mean() const
			{
				const auto mean = sum / static_cast<double>(lists);
				return static_cast<float>(std::round(mean / AREA_STEP) * AREA_STEP);
			}
		};

		// Adds every line of a_container that is a leveled list ranked for
		// weapons, each line once. Returns how many it added. A plain item adds
		// nothing, since Supply ranks lists and not items.
		std::size_t AddLines(const Supply& a_supply, const RE::TESContainer& a_container, Tally& a_tally)
		{
			std::size_t added = 0;
			for (const auto* entry : std::span{ a_container.containerObjects, a_container.numContainerObjects }) {
				if (!entry || !entry->obj || !entry->obj->Is(RE::ENUM_FORM_ID::kLVLI)) {
					continue;
				}
				if (const auto rank = SupplyOf(a_supply, entry->obj->formID, Condition::Kind::kWeapon)) {
					a_tally.sum += *rank;
					++a_tally.lists;
					++added;
				}
			}
			return added;
		}

		// Adds 1 record's mean to the ladder, when it starts with at least 1
		// weapon list. Records the game created in this game are left out, so
		// the ladder is the load order's.
		void AddRecord(const Supply& a_supply, const RE::TESForm* a_record, const RE::TESContainer& a_container, std::vector<float>& a_ladder)
		{
			if (a_record->IsCreated()) {
				return;
			}
			Tally tally;
			if (AddLines(a_supply, a_container, tally) > 0) {
				a_ladder.push_back(tally.Mean());
			}
		}

		// -------------------------------------------------------------------
		// The walk of 1 cell
		// -------------------------------------------------------------------

		// What 1 cell's containers and characters start with. busy and
		// detached each say the walk counted nothing.
		struct Walk
		{
			bool        busy{ false };
			bool        detached{ false };
			Tally       tally;
			std::size_t containers{ 0 };
			std::size_t characters{ 0 };
			double      ms{ 0.0 };
		};

		// The first entry of a leveled list that no script added, so the list
		// counts as the plugins made it.
		const RE::TESForm* FirstEntry(const RE::TESLeveledList& a_list)
		{
			const auto entries = static_cast<std::size_t>(a_list.baseListCount);
			for (std::size_t i = 0; a_list.leveledLists && i < entries; ++i) {
				const auto& entry = a_list.leveledLists[i];
				if (entry.form && !a_list.IsScriptAdded(&entry)) {
					return entry.form;
				}
			}
			return nullptr;
		}

		// The character a placed leveled character stands for before the
		// engine resolved it: its first entry, followed down through lists in
		// lists, at most DEEPEST steps. Once resolved, the reference's base is
		// the engine's copy, which carries the template's inventory.
		const RE::TESNPC* FirstLeaf(const RE::TESLevCharacter& a_list)
		{
			const RE::TESLevCharacter* list = &a_list;
			for (std::size_t depth = 0; depth < DEEPEST; ++depth) {
				const auto* form = FirstEntry(*list);
				if (!form) {
					return nullptr;
				}
				if (form->Is(RE::ENUM_FORM_ID::kNPC_)) {
					return static_cast<const RE::TESNPC*>(form);
				}
				if (!form->Is(RE::ENUM_FORM_ID::kLVLN)) {
					return nullptr;
				}
				list = static_cast<const RE::TESLevCharacter*>(form);
			}
			return nullptr;
		}

		// Adds the weapon lists of every container and character in a_cell,
		// as they stand in the record, loot taken or not. Disabled references
		// count, so a quest that enables a squad moves nothing. The player,
		// companions and anything the game created in this game do not.
		//
		// The game moves an exterior's persistent references into the cell's
		// array as the cell loads, by where they stand
		// (TESWorldSpace::LoadExteriorCell), so a named character at home
		// counts.
		//
		// The engine takes cell and inventory locks in both orders, and the
		// pickup rolls while it holds the receiver's inventory. So this tries
		// the cell's lock once and never waits, and while it holds the lock it
		// reads plain fields only and calls nothing that locks.
		//
		// Only an attached cell is walked, the state Papyrus's Cell.IsAttached
		// reads on both games. A cell that is not loaded holds only its
		// persistent references, and one still loading holds part of the
		// rest, so either would be kept as too poor a place. A loaded cell
		// outside the attached area, such as an interior just left, is
		// skipped the same way.
		Walk WalkCell(const Supply& a_supply, const RE::TESObjectCELL& a_cell)
		{
			Walk        walk;
			const auto* player = RE::PlayerCharacter::GetSingleton();

			// The lock is the cell's own and the walk only reads.
			auto&            lock = const_cast<RE::BSSpinLock&>(a_cell.spinLock);
			std::unique_lock held{ lock, std::try_to_lock };
			if (!held) {
				walk.busy = true;
				return walk;
			}
			if (!a_cell.IsAttached()) {
				walk.detached = true;
				return walk;
			}

			const auto start = std::chrono::steady_clock::now();
			for (const auto& pointer : a_cell.references) {
				auto* ref = pointer.get();
				if (!ref || ref->IsDeleted() || ref->IsCreated() || ref == player) {
					continue;
				}
				if (const auto* actor = ref->As<RE::Actor>(); actor && actor->IsPlayerTeammate()) {
					continue;
				}

				const auto* base = ref->GetObjectReference();
				if (!base) {
					continue;
				}
				if (base->Is(RE::ENUM_FORM_ID::kCONT)) {
					if (AddLines(a_supply, static_cast<const RE::TESObjectCONT&>(*base), walk.tally) > 0) {
						++walk.containers;
					}
					continue;
				}

				const RE::TESNPC* npc = nullptr;
				if (base->Is(RE::ENUM_FORM_ID::kNPC_)) {
					npc = static_cast<const RE::TESNPC*>(base);
				} else if (base->Is(RE::ENUM_FORM_ID::kLVLN)) {
					npc = FirstLeaf(static_cast<const RE::TESLevCharacter&>(*base));
				}
				if (npc && AddLines(a_supply, *npc, walk.tally) > 0) {
					++walk.characters;
				}
			}
			held.unlock();
			walk.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
			return walk;
		}

		// -------------------------------------------------------------------
		// The cache
		// -------------------------------------------------------------------

		// The twin of SpawnCondition::NameOf, which this layer may not include.
		std::string_view NameOf(const RE::TESObjectCELL& a_cell)
		{
			const auto name = RE::TESFullName::GetFullName(a_cell);
			return name.empty() ? std::string_view{ "unnamed cell" } : name;
		}

		std::string_view Plural(std::size_t a_count, std::string_view a_one, std::string_view a_many)
		{
			return a_count == 1 ? a_one : a_many;
		}

		// Keeps a_rank for the cell unless another thread measured it first,
		// whose answer stays. Returns the answer kept and whether it was this.
		std::pair<float, bool> Keep(const Areas& a_areas, RE::TESFormID a_cell, float a_rank)
		{
			const std::scoped_lock hold{ a_areas.lock };
			const auto [kept, added] = a_areas.ranks.try_emplace(a_cell, a_rank);
			return { kept->second, added };
		}
	}

	std::size_t MeasureAreas(const Supply& a_supply, Areas& a_out)
	{
		auto& ladder = a_out.ladder;
		for (const auto* container : g_dataHandler->GetFormArray<RE::TESObjectCONT>()) {
			if (container) {
				AddRecord(a_supply, container, *container, ladder);
			}
		}
		for (const auto* npc : g_dataHandler->GetFormArray<RE::TESNPC>()) {
			if (npc) {
				AddRecord(a_supply, npc, *npc, ladder);
			}
		}
		std::sort(ladder.begin(), ladder.end());
		return ladder.size();
	}

	std::optional<float> AreaOf(const Supply& a_supply, const Areas& a_areas, const RE::TESObjectCELL& a_cell)
	{
		{
			const std::scoped_lock hold{ a_areas.lock };
			if (const auto found = a_areas.ranks.find(a_cell.formID); found != a_areas.ranks.end()) {
				return found->second == UNMEASURED ? std::nullopt : std::optional{ found->second };
			}
		}

		// No NEC lock is held while the game's memory is walked, so 2 threads
		// may measure the same cell at once.
		const auto walk = WalkCell(a_supply, a_cell);

		// Nothing is kept, so the next roll in the cell tries again.
		if (walk.busy || walk.detached) {
			if (TraceLog::IsOpen()) {
				TraceLog::Line("area", "{:s} [{:08X}] {:s}, so this roll takes the ordinary middle and the area is measured at its next roll",
					NameOf(a_cell), a_cell.formID, walk.busy ? "was busy" : "is not attached");
			}
			return std::nullopt;
		}

		const auto rank = walk.tally.lists > 0 ? PositionOn(a_areas.ladder, walk.tally.Mean()) : UNMEASURED;
		const auto [kept, added] = Keep(a_areas, a_cell.formID, rank);
		if (added && TraceLog::IsOpen()) {
			if (walk.tally.lists == 0) {
				TraceLog::Line("area", "{:s} [{:08X}] has no leveled list of weapons in a container or character, so loot lying in it rolls at the ordinary middle",
					NameOf(a_cell), a_cell.formID);
			} else {
				TraceLog::Line("area", "{:s} [{:08X}] ranks {:.2f}, from {:d} leveled {:s} in {:d} {:s} and {:d} {:s}, measured in {:.1f} ms",
					NameOf(a_cell), a_cell.formID, rank,
					walk.tally.lists, Plural(walk.tally.lists, "list", "lists"),
					walk.containers, Plural(walk.containers, "container", "containers"),
					walk.characters, Plural(walk.characters, "character", "characters"),
					walk.ms);
			}
		}
		return kept == UNMEASURED ? std::nullopt : std::optional{ kept };
	}
}
