#include "Condition/Provenance/Provenance.h"

#include "Condition/Provenance/Care.h"
#include "Condition/Provenance/Rank.h"
#include "Condition/Provenance/Supply.h"

#include <atomic>
#include <cmath>

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

		// Whether the maps MeasureSupply and MeasureCare fill are finished and
		// safe to read. The spawn hook is live from the moment the plugin
		// loads, before Load has run, and several threads build inventories at
		// once, so without this a spawn could read a half written map and
		// crash. Set once both maps are complete, cleared by Unload before they
		// are emptied. Release and acquire make a reader that sees true also
		// see everything written before it. A reader that sees false rolls the
		// weapon at the plain middle. It only goes back to false when the game
		// deletes every form, when nothing is building an inventory.
		std::atomic<bool> g_measured{ false };
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
		// Readers are stopped first, so none starts on a map being emptied.
		g_measured.store(false, std::memory_order_release);

		ForgetSupply();
		ForgetCare();
	}

	void Load()
	{
		// The store at the end publishes the maps to running threads. Load runs
		// before any inventory is built, and again only after Unload has
		// stopped every reader.
		Unload();
		if (!g_dataHandler) {
			return;
		}

		// The care half reads back the armor the supply half measured for each
		// leveled list, so the supply half goes first.
		const auto supply = MeasureSupply();
		const auto care = MeasureCare();

		g_measured.store(true, std::memory_order_release);

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

		// Nothing measured yet. Reading a map while it is being built is a
		// crash, not an out of date number.
		if (!g_measured.load(std::memory_order_acquire)) {
			return origin;
		}

		// Which leveled list produced this item. The engine resolves a
		// reference's starting inventory list and stamps the list's form ID
		// into an ExtraLeveledItem on everything that came out of it, before
		// any of it reaches an inventory.
		if (a_stack.extra) {
			if (const auto* came = a_stack.extra->GetByType<RE::ExtraLeveledItem>()) {
				if (const auto supply = SupplyOf(came->levItem, a_kind)) {
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
			origin.care = CareOf(npc);
		}

		return origin;
	}
}
