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

		// How much armor 1 character is given, a_outfit and own inventory
		// together. A raider's armor and a gunner's gear are not outfit items
		// but leveled lists in the inventory, beside the gun. Only the record
		// itself is read, see Issued in Equipped.cpp for why a template needs no
		// walk. Returns nothing when any part could not be worked out, so nobody
		// is ranked on part of their gear. Only reads, so the spawn hook can
		// call it on any thread.
		std::optional<float> ArmorIssued(const Supply& a_supply, const RE::TESNPC& a_npc, const RE::BGSOutfit* a_outfit)
		{
			float armor = 0.0F;

			if (a_outfit) {
				for (const auto* piece : a_outfit->outfitItems) {
					if (!piece) {
						continue;
					}

					const auto worth = ArmorFrom(a_supply, *piece);
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

				const auto worth = ArmorFrom(a_supply, *entry->obj);
				if (!worth) {
					return std::nullopt;
				}
				armor += *worth;
			}

			return armor;
		}

		// Where 1 armor total ranks among the characters the last measure
		// ranked, the same answer PositionsOf gives, for someone it never saw.
		float PositionIn(const Care& a_care, float a_armor)
		{
			const auto& ladder = a_care.ladder;
			if (ladder.size() < 2) {
				return UNKNOWN_HALF;
			}

			const auto first = std::lower_bound(ladder.begin(), ladder.end(), a_armor) - ladder.begin();
			const auto after = std::upper_bound(ladder.begin(), ladder.end(), a_armor) - ladder.begin();
			const auto middle = (static_cast<float>(first) + static_cast<float>(after - 1)) * 0.5F;
			return std::clamp(middle / static_cast<float>(ladder.size() - 1), 0.0F, 1.0F);
		}

		// The outfit a character is measured by: the record's own, or for a
		// character whose outfit a script changed in this game, as
		// Actor.SetOutfit and the console's setoutfit do, the one the measure
		// before counted. The save brings the script's outfit back at every
		// load, and the one the plugins and DLL mods gave cannot be read any
		// more. Their own inventory is read as it is now either way.
		const RE::BGSOutfit* OutfitCounted(const RE::TESNPC& a_npc, const Care* a_last)
		{
			if (!a_last) {
				return a_npc.defOutfit;
			}
			const auto  last = a_last->issued.find(&a_npc);
			const auto* save = RE::BGSSaveLoadGame::GetSingleton();
			if (last == a_last->issued.end() || !save ||
				!save->HasChangeFlags(&a_npc, RE::CHANGE_TYPES::kNPCDefaultOutfit)) {
				return a_npc.defOutfit;
			}
			return last->second.outfit;
		}
	}

	CareCount MeasureCare(const Supply& a_supply, const Care* a_last, Care& a_out)
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

			const auto* outfit = OutfitCounted(*npc, a_last);
			const auto  armor = ArmorIssued(a_supply, *npc, outfit);
			a_out.issued.emplace(npc, Issued{ outfit, armor });
			if (!armor) {
				++spoiled;
			} else if (*armor > 0.0F) {
				kits.emplace_back(npc, OnARung(*armor));
			} else {
				issuedNothing.push_back(npc);
			}
		}

		a_out.ranks = PositionsOf(kits);

		// The same totals sorted, so a character the engine creates later can
		// be ranked.
		auto& ladder = a_out.ladder;
		ladder.reserve(kits.size());
		for (const auto& [npc, armor] : kits) {
			ladder.push_back(armor);
		}
		std::sort(ladder.begin(), ladder.end());

		for (const auto* npc : issuedNothing) {
			a_out.ranks.emplace(npc, 0.0F);
		}
		for (const auto* npc : armored) {
			a_out.ranks.emplace(npc, 1.0F);
		}

		return { a_out.ranks.size(), kits.size(), issuedNothing.size(), armored.size(), spoiled };
	}

	float CareOf(const Supply& a_supply, const Care& a_care, const RE::TESNPC& a_npc)
	{
		float care = UNMEASURED;

		// A record that was loaded when the measuring happened, every settler
		// and every named character: one lookup.
		const auto found = a_care.ranks.find(&a_npc);
		if (found != a_care.ranks.end()) {
			care = found->second;

		// Otherwise the engine created them while the game ran. A raider or a
		// gunner is rolled from a leveled character list into a new record with
		// a form ID starting FF. The copy has its outfit and inventory filled
		// in, so it is measured on the spot from what the last measure worked
		// out and lands on the same scale.
		} else if (InPowerArmor(a_npc)) {
			care = 1.0F;
		} else if (const auto armor = ArmorIssued(a_supply, a_npc, a_npc.defOutfit)) {
			care = *armor > 0.0F ? PositionIn(a_care, OnARung(*armor)) : 0.0F;
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
