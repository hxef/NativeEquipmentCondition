#include "Condition/Provenance/Provenance.h"

#include "Condition/Provenance/Care.h"
#include "Condition/Provenance/Rank.h"
#include "Condition/Provenance/Supply.h"
#include "Core/Snapshot.h"

#include <cmath>
#include <format>
#include <memory>
#include <string>
#include <vector>

namespace Provenance
{
	namespace
	{
		// -------------------------------------------------------------------
		// The 2 halves, and what is kept of each
		// -------------------------------------------------------------------

		// The best a weapon can be from the poorest source in the load order,
		// and from the richest. A ceiling, since what happened to it later is
		// the other half.
		constexpr float SUPPLY_FLOOR = 0.60F;
		constexpr float SUPPLY_CEILING = 1.00F;

		// How much of that ceiling survives an owner who looks after nothing,
		// and an owner who looks after everything.
		constexpr float CARE_FLOOR = 0.55F;
		constexpr float CARE_CEILING = 1.00F;

		// Both halves of 1 measure. The spawn hook is live from the moment the
		// plugin loads and several threads build inventories at once, so every
		// reader takes the ranking whole, see Core/Snapshot.h, and with none
		// rolls the item at the plain middle.
		struct Ranking
		{
			Supply supply;
			Care   care;
		};
		Snapshot<Ranking> g_ranking;

		// The care half reads back the armor the supply half measured for each
		// leveled list, so the supply half goes first.
		std::shared_ptr<Ranking> Measure(const Ranking* a_last, SupplyCount& a_supply, CareCount& a_care)
		{
			auto ranking = std::make_shared<Ranking>();
			a_supply = MeasureSupply(ranking->supply);
			a_care = MeasureCare(ranking->supply, a_last ? &a_last->care : nullptr, ranking->care);
			return ranking;
		}
	}

	float Origin::Centre() const
	{
		const auto fromPipeline = supply == UNMEASURED ? UNKNOWN_HALF : supply;
		const auto fromKeeper = care == UNMEASURED ? UNKNOWN_HALF : care;

		// Multiplied, not averaged. Supply is how good the weapon was when this
		// owner got it, care is how much of that is left. A raider who looted a
		// Gauss rifle is still a raider.
		return std::lerp(SUPPLY_FLOOR, SUPPLY_CEILING, fromPipeline) *
			   std::lerp(CARE_FLOOR, CARE_CEILING, fromKeeper);
	}

	float LowestCentre()
	{
		return SUPPLY_FLOOR * CARE_FLOOR;
	}

	float HighestCentre()
	{
		return SUPPLY_CEILING * CARE_CEILING;
	}

	void Unload()
	{
		g_ranking.Publish(nullptr);
	}

	void Load()
	{
		Unload();
		if (!g_dataHandler) {
			return;
		}

		SupplyCount supply{};
		CareCount   care{};
		g_ranking.Publish(Measure(nullptr, supply, care));

		REX::INFO("Ranked {:d} leveled lists that hand out weapons, {:d} that hand out armor, and what {:d} characters are issued. Of those, {:d} are issued armor and ranked against each other, {:d} are issued none, and {:d} spawn in power armor.",
			supply.ranked, supply.armorRanked, care.ranked, care.kits, care.issuedNothing, care.armored);

		// Loud on purpose. A leveled list in this load order points round in a
		// circle or nests absurdly deep, and whatever draws from it has lost
		// its ranking.
		const auto spoiled = supply.spoiled + care.spoiled;
		if (spoiled > 0) {
			REX::WARN("{:d} leveled lists and outfits could not be worked out, because they lead round in a circle or nest deeper than {:d}. Whatever draws from them spawns at an ordinary condition.",
				spoiled, DEEPEST);
		}
	}

	Origin Of(const RE::BGSInventoryList& a_list, const RE::BGSInventoryItem::Stack& a_stack, Condition::Kind a_kind)
	{
		Origin origin;

		const auto ranking = g_ranking.Get();
		if (!ranking) {
			return origin;
		}

		// Which leveled list produced this item. The engine resolves a
		// reference's starting inventory list and stamps the list's form ID
		// into an ExtraLeveledItem on everything that came out of it, before
		// any of it reaches an inventory.
		if (a_stack.extra) {
			if (const auto* came = a_stack.extra->GetByType<RE::ExtraLeveledItem>()) {
				if (const auto supply = SupplyOf(ranking->supply, came->levItem, a_kind)) {
					origin.supply = *supply;
					origin.pipeline = came->levItem;
				}
			}
		}

		// Who the inventory belongs to. The list holds a handle to its owner
		// from the moment it is built, so this works even while an actor's
		// inventory is filled for the first time. Only an actor has this half.
		// A footlocker looks after nothing.
		const auto  owner = a_list.owner.get();
		const auto* base = owner ? owner->GetObjectReference() : nullptr;
		if (base && base->Is(RE::ENUM_FORM_ID::kNPC_)) {
			const auto& npc = static_cast<const RE::TESNPC&>(*base);

			// Kept whether or not the half was measured, so the trace can tell
			// a character this failed on from a footlocker.
			origin.keeper = &npc;
			origin.care = CareOf(ranking->supply, ranking->care, npc);
		}

		return origin;
	}

	std::vector<std::string> MeasureAgain()
	{
		const auto was = g_ranking.Get();
		if (!was) {
			return {};
		}
		SupplyCount supply{};
		CareCount   care{};
		auto        now = Measure(was.get(), supply, care);

		// What went into the ranks and not the ranks, since 1 list or
		// character that moves shifts the place of every one ranked between.
		std::vector<std::string> moved;
		if (const auto lists = ChangedKeys(was->supply.hauls, now->supply.hauls); lists > 0) {
			moved.push_back(std::format("what {:d} leveled lists hand out", lists));
		}
		if (const auto characters = ChangedKeys(was->care.issued, now->care.issued); characters > 0) {
			moved.push_back(std::format("what {:d} characters are issued", characters));
		}
		g_ranking.Publish(std::move(now));
		return moved;
	}
}
