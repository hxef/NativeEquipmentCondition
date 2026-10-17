#include "Condition/Provenance/Care.h"

#include "Condition/Provenance/Provenance.h"
#include "Condition/Provenance/Rank.h"
#include "Condition/Provenance/Supply.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>

namespace Provenance
{
	namespace
	{
		// Where a unique or protected character lands when nothing else is
		// known, and how far towards the top the boost moves one who was
		// measured.
		constexpr float NAMED_CARE = 0.75F;
		constexpr float NAMED_LIFT = 0.35F;

		// Where each character ranks against the rest by the armor they are
		// given. Keyed by base form, which the owner of an inventory points to
		// directly.
		std::unordered_map<const RE::TESNPC*, float> g_care;

		// Every ranked character's armor total, sorted, so a character who was
		// not there to be ranked can be placed among them.
		std::vector<float> g_ladder;

		// -------------------------------------------------------------------
		// What a character is given
		// -------------------------------------------------------------------

		// Whether this character spawns in power armor. That comes from neither
		// the outfit nor the inventory, but from a furniture reference the
		// character stands inside, with no rating to add up. The game copies a
		// template's power armor onto the record as it loads.
		bool InPowerArmor(const RE::TESNPC& a_npc)
		{
			return a_npc.powerArmorFurn != nullptr;
		}

		// How much armor one character is given, outfit and own inventory
		// together. A raider's armor and a gunner's gear are not outfit items
		// but leveled lists in the inventory, beside the gun. Only the record
		// itself is read, see Issued in Equipped.cpp for why a template needs no
		// walk. Returns nothing when any part could not be worked out, so nobody
		// is ranked on part of their gear. Only reads: every list was measured
		// at Load, so the spawn hook can call this on any thread.
		std::optional<float> ArmorIssued(const RE::TESNPC& a_npc)
		{
			float armor = 0.0F;

			if (const auto* outfit = a_npc.defOutfit) {
				for (const auto* piece : outfit->outfitItems) {
					if (!piece) {
						continue;
					}

					const auto worth = ArmorFrom(*piece);
					if (!worth) {
						return std::nullopt;
					}
					armor += *worth;
				}
			}

			for (const auto* entry : std::span{ a_npc.containerObjects, a_npc.numContainerObjects }) {
				if (!entry || !entry->obj) {
					continue;
				}

				const auto worth = ArmorFrom(*entry->obj);
				if (!worth) {
					return std::nullopt;
				}
				armor += *worth;
			}

			return armor;
		}

		// Where one armor total ranks among the characters ranked at startup,
		// the same answer PositionsOf gives, for someone it never saw.
		float PositionIn(float a_armor)
		{
			if (g_ladder.size() < 2) {
				return UNKNOWN_HALF;
			}

			const auto first = std::lower_bound(g_ladder.begin(), g_ladder.end(), a_armor) - g_ladder.begin();
			const auto after = std::upper_bound(g_ladder.begin(), g_ladder.end(), a_armor) - g_ladder.begin();
			const auto middle = (static_cast<float>(first) + static_cast<float>(after - 1)) * 0.5F;
			return std::clamp(middle / static_cast<float>(g_ladder.size() - 1), 0.0F, 1.0F);
		}
	}

	CareCount MeasureCare()
	{
		std::size_t spoiled = 0;

		// Every character ranked by the armor they are given, in 3 groups.
		// Characters given some armor are ranked against each other. Characters
		// given none go to the bottom, since most records are settlers,
		// shopkeepers and children, and ranking them would squeeze everyone who
		// matters into the top half, a raider and a farmer 0.01 apart.
		// Characters in power armor go to the top, since there is no rating to
		// add up.
		std::vector<std::pair<const RE::TESNPC*, float>> kits;
		std::vector<const RE::TESNPC*>                  issuedNothing;
		std::vector<const RE::TESNPC*>                  armored;
		for (const auto* npc : g_dataHandler->GetFormArray<RE::TESNPC>()) {
			if (!npc) {
				continue;
			}

			if (InPowerArmor(*npc)) {
				armored.push_back(npc);
				continue;
			}

			const auto armor = ArmorIssued(*npc);
			if (!armor) {
				++spoiled;
			} else if (*armor > 0.0F) {
				kits.emplace_back(npc, OnARung(*armor));
			} else {
				issuedNothing.push_back(npc);
			}
		}

		g_care = PositionsOf(kits);

		// The same totals sorted, so a character the engine creates later can
		// be ranked.
		g_ladder.clear();
		g_ladder.reserve(kits.size());
		for (const auto& [npc, armor] : kits) {
			g_ladder.push_back(armor);
		}
		std::sort(g_ladder.begin(), g_ladder.end());

		for (const auto* npc : issuedNothing) {
			g_care.emplace(npc, 0.0F);
		}
		for (const auto* npc : armored) {
			g_care.emplace(npc, 1.0F);
		}

		return { g_care.size(), kits.size(), issuedNothing.size(), armored.size(), spoiled };
	}

	void ForgetCare()
	{
		g_care.clear();
		g_ladder.clear();
	}

	float CareOf(const RE::TESNPC& a_npc)
	{
		float care = UNMEASURED;

		// A record that was loaded when the measuring happened, every settler
		// and every named character: one lookup.
		const auto found = g_care.find(&a_npc);
		if (found != g_care.end()) {
			care = found->second;

		// Otherwise the engine created them while the game ran. A raider or a
		// gunner is rolled from a leveled character list into a new record with
		// a form ID starting FF. The copy has its outfit and inventory filled
		// in, so it is measured on the spot from what Load worked out and lands
		// on the same scale.
		} else if (InPowerArmor(a_npc)) {
			care = 1.0F;
		} else if (const auto armor = ArmorIssued(a_npc)) {
			care = *armor > 0.0F ? PositionIn(OnARung(*armor)) : 0.0F;
		}

		// A unique or protected character gets a boost, since someone the game
		// treats as special has their gear looked after. An ordinary raider is
		// neither and a raider boss is. An essential character's gear arrives
		// new instead, see SpawnCondition/Roll.cpp.
		using Flag = RE::ACTOR_BASE_DATA::Flag;
		if (a_npc.actorData.actorBaseFlags.any(Flag::kUnique, Flag::kProtected)) {
			care = care == UNMEASURED ? NAMED_CARE : std::lerp(care, 1.0F, NAMED_LIFT);
		}

		return care;
	}
}
