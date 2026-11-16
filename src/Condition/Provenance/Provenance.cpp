#include "Condition/Provenance/Provenance.h"

#include "Condition/Provenance/Area.h"
#include "Condition/Provenance/Care.h"
#include "Condition/Provenance/Rank.h"
#include "Condition/Provenance/Supply.h"
#include "Core/Settings/Settings.h"
#include "Core/Snapshot.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
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

		// Both halves of 1 measure, and the areas measured from the supply
		// half. The spawn hook is live from the moment the plugin loads and
		// several threads build inventories at once, so every reader takes the
		// ranking whole, see Core/Snapshot.h, and with none rolls the item at
		// the plain middle. The 3 are shared, so a move of fLootAreaEffect to
		// or from off swaps in a ranking with new areas or none and keeps both
		// halves.
		struct Ranking
		{
			std::shared_ptr<const Supply> supply;
			std::shared_ptr<const Care>   care;
			std::shared_ptr<const Areas>  areas;  // nothing while fLootAreaEffect is off
		};
		Snapshot<Ranking> g_ranking;

		// Held from reading the last ranking to publishing the next, so a
		// save load's measure and a move of the slider on the MCM page never
		// build on the same ranking and drop the other's work.
		std::mutex g_measuring;

		// fLootAreaEffect multiplies the built rule, 1 is the rule as built
		// and 5 the most it counts.
		constexpr float MOST_AREA_EFFECT = 5.0F;
		constexpr float BUILT_AREA_EFFECT = 1.0F;

		// The slider moves in steps of 0.1, so a value under 0.05 shows as 0
		// and counts as off.
		constexpr float LEAST_AREA_EFFECT = 0.05F;

		// fLootAreaEffect as it counts, 0 to 5, and exactly 0 while it is off.
		// A number that is no number counts as off too. The only test of off,
		// so the areas, the roll and NEC.log agree.
		float AreaEffect()
		{
			const auto effect = Settings::fLootAreaEffect.GetValue();
			return effect >= LEAST_AREA_EFFECT ? std::min(effect, MOST_AREA_EFFECT) : 0.0F;
		}

		// The areas measured against a_supply, or nothing while
		// fLootAreaEffect is off, so no ladder is built.
		std::shared_ptr<const Areas> AreasOf(const Supply& a_supply)
		{
			if (AreaEffect() == 0.0F) {
				return nullptr;
			}
			auto areas = std::make_shared<Areas>();
			MeasureAreas(a_supply, *areas);
			return areas;
		}

		// The care half and the areas read back what the supply half measured
		// for each leveled list, so the supply half goes first.
		std::shared_ptr<const Ranking> Measure(const Ranking* a_last, SupplyCount& a_supply, CareCount& a_care)
		{
			auto supply = std::make_shared<Supply>();
			a_supply = MeasureSupply(*supply);
			auto areas = AreasOf(*supply);
			auto care = std::make_shared<Care>();
			a_care = MeasureCare(*supply, a_last ? a_last->care.get() : nullptr, *care);
			return std::make_shared<const Ranking>(Ranking{ std::move(supply), std::move(care), std::move(areas) });
		}

		// Where loose loot rolls, for NEC.log at each load and each time the
		// slider turns it on or off.
		void SayAreas(const Ranking& a_ranking)
		{
			if (a_ranking.areas) {
				REX::INFO("Ranked {:d} containers and characters by the weapons they start with. Loose loot rolls by where its area falls among them.",
					a_ranking.areas->ladder.size());
			} else {
				REX::INFO("Loose loot rolls the same everywhere, since fLootAreaEffect is {:g}.",
					Settings::fLootAreaEffect.GetValue());
			}
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
		const std::scoped_lock measuring{ g_measuring };
		g_ranking.Publish(nullptr);
	}

	void Load()
	{
		Unload();
		if (!g_dataHandler) {
			return;
		}

		const std::scoped_lock measuring{ g_measuring };
		SupplyCount            supply{};
		CareCount              care{};
		const auto             now = Measure(nullptr, supply, care);
		g_ranking.Publish(now);

		REX::INFO("Ranked {:d} leveled lists that hand out weapons, {:d} that hand out armor, and what {:d} characters are issued. Of those, {:d} are issued armor and ranked against each other, {:d} are issued none, and {:d} spawn in power armor.",
			supply.ranked, supply.armorRanked, care.ranked, care.kits, care.issuedNothing, care.armored);
		SayAreas(*now);

		// Loud on purpose. A leveled list in this load order points round in a
		// circle or nests absurdly deep, and whatever draws from it has lost
		// its ranking.
		const auto spoiled = supply.spoiled + care.spoiled;
		if (spoiled > 0) {
			REX::WARN("{:d} leveled lists and outfits could not be worked out, because they lead round in a circle or nest deeper than {:d}. Whatever draws from them spawns at an ordinary condition.",
				spoiled, DEEPEST);
		}
	}

	Origin Of(const RE::BGSInventoryList& a_list, const RE::ExtraDataList* a_extra, Condition::Kind a_kind, const RE::TESObjectCELL* a_cell)
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
		if (a_extra) {
			if (const auto* came = a_extra->GetByType<RE::ExtraLeveledItem>()) {
				if (const auto supply = SupplyOf(*ranking->supply, came->levItem, a_kind)) {
					origin.supply = *supply;
					origin.pipeline = came->levItem;
				}
			}
		}

		// A ranked stamped list always wins, and the area stands in only when
		// no list ranked for this kind handed the item out. While
		// fLootAreaEffect is off no cell is walked, and the item rolls at the
		// ordinary middle, as in a cell with no list. The slider is read too,
		// since the page sets it a moment before FollowAreaEffect drops the
		// areas. The slider multiplies how far the area's rank lies from the
		// middle, never past 0 or 1. 1 takes the rank untouched, so the
		// default rolls bit for bit as built.
		const auto effect = AreaEffect();
		if (origin.supply == UNMEASURED && a_cell && ranking->areas && effect > 0.0F) {
			if (const auto area = AreaOf(*ranking->supply, *ranking->areas, *a_cell)) {
				origin.supply = effect == BUILT_AREA_EFFECT ?
				                    *area :
				                    std::clamp(UNKNOWN_HALF + (*area - UNKNOWN_HALF) * effect, 0.0F, 1.0F);
				origin.pipeline = a_cell->formID;
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
			origin.care = CareOf(*ranking->supply, *ranking->care, npc);
		}

		return origin;
	}

	std::vector<std::string> MeasureAgain()
	{
		const std::scoped_lock measuring{ g_measuring };
		const auto             was = g_ranking.Get();
		if (!was) {
			return {};
		}
		SupplyCount supply{};
		CareCount   care{};
		auto        now = Measure(was.get(), supply, care);

		// What went into the ranks and not the ranks, since 1 list or
		// character that moves shifts the place of every one ranked between.
		std::vector<std::string> moved;
		if (const auto lists = ChangedKeys(was->supply->hauls, now->supply->hauls); lists > 0) {
			moved.push_back(std::format("what {:d} {:s}", lists, lists == 1 ? "leveled list hands out" : "leveled lists hand out"));
		}
		if (const auto characters = ChangedKeys(was->care->issued, now->care->issued); characters > 0) {
			moved.push_back(std::format("what {:d} {:s}", characters, characters == 1 ? "character is issued" : "characters are issued"));
		}
		g_ranking.Publish(std::move(now));
		return moved;
	}

	void FollowAreaEffect()
	{
		const std::scoped_lock measuring{ g_measuring };
		auto                   was = g_ranking.Get();
		if (!was || (was->areas != nullptr) == (AreaEffect() > 0.0F)) {
			return;
		}
		const auto now = std::make_shared<const Ranking>(Ranking{ was->supply, was->care, AreasOf(*was->supply) });
		g_ranking.Publish(now);
		SayAreas(*now);

		// On a move to off the old ranking's ladder and kept cells go now and
		// not at the next publish. This copy goes first, or the old ranking
		// is held twice and LetGo keeps it. Both halves live on in now.
		if (!now->areas) {
			was.reset();
			g_ranking.LetGo();
		}
	}
}
