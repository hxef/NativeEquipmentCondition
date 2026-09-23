#include "Condition/Provenance/Care.h"

#include "Condition/Provenance/Provenance.h"
#include "Condition/Provenance/Rank.h"
#include "Condition/Provenance/Supply.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace Provenance
{
	namespace
	{
		// Where a unique, essential or protected character lands when nothing
		// else is known, and how far towards the top the boost moves one who
		// was measured.
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

		// How deep a chain of templates is followed. Vanilla rarely goes past
		// 2, so this only stops a plugin whose templates point round in a
		// circle.
		constexpr std::size_t TEMPLATES = 8;

		// The outfit a character is given. The engine reads defOutfit straight
		// from the record, so a record holding one is the whole answer. Leveled
		// raiders and gunners are records with little of their own that take
		// their gear from a template, and it is unclear whether the engine has
		// copied the template's outfit down by the time a weapon arrives, so
		// the chain is followed when the record says it takes its inventory
		// from a template. A character using a template for stats alone, with
		// an empty outfit, wears nothing.
		const RE::BGSOutfit* OutfitOf(const RE::TESNPC& a_npc)
		{
			using Flag = RE::ACTOR_BASE_DATA::Flag;
			using Use = RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG;

			const RE::TESNPC* npc = &a_npc;
			for (std::size_t hop = 0; npc && hop < TEMPLATES; ++hop) {
				if (npc->defOutfit) {
					return npc->defOutfit;
				}

				if (!npc->actorData.actorBaseFlags.any(Flag::kUsesTemplate) ||
					!npc->actorData.templateUseFlags.any(Use::kInventory)) {
					return nullptr;
				}

				// A template can be a leveled character, a list to resolve and
				// not a record to read, so the chain stops there.
				const auto* next = npc->baseTemplateForm;
				npc = next && next->Is(RE::ENUM_FORM_ID::kNPC_) ?
				          static_cast<const RE::TESNPC*>(next) :
				          nullptr;
			}
			return nullptr;
		}

		// The record a character's own inventory is written on. Works like
		// OutfitOf: a leveled raider carries nothing of its own.
		const RE::TESNPC* CarriedBy(const RE::TESNPC& a_npc)
		{
			using Flag = RE::ACTOR_BASE_DATA::Flag;
			using Use = RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG;

			const RE::TESNPC* npc = &a_npc;
			for (std::size_t hop = 0; npc && hop < TEMPLATES; ++hop) {
				if (npc->numContainerObjects > 0) {
					return npc;
				}

				if (!npc->actorData.actorBaseFlags.any(Flag::kUsesTemplate) ||
					!npc->actorData.templateUseFlags.any(Use::kInventory)) {
					return nullptr;
				}

				const auto* next = npc->baseTemplateForm;
				npc = next && next->Is(RE::ENUM_FORM_ID::kNPC_) ?
				          static_cast<const RE::TESNPC*>(next) :
				          nullptr;
			}
			return nullptr;
		}

		// Whether this character spawns in power armor. That comes from neither
		// the outfit nor the inventory, but from a furniture reference the
		// character stands inside, with no rating to add up.
		bool InPowerArmor(const RE::TESNPC& a_npc)
		{
			if (a_npc.powerArmorFurn) {
				return true;
			}

			const auto* carried = CarriedBy(a_npc);
			return carried && carried->powerArmorFurn;
		}

		// How much armor one character is given, outfit and own inventory
		// together. A raider's armor and a gunner's gear are not outfit items
		// but leveled lists in the inventory, beside the gun. Returns nothing
		// when any part could not be worked out, so nobody is ranked on part of
		// their gear. Only reads: every list was measured at Load, so the spawn
		// hook can call this on any thread.
		std::optional<float> ArmorIssued(const RE::TESNPC& a_npc)
		{
			float armor = 0.0F;

			if (const auto* outfit = OutfitOf(a_npc)) {
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

			const auto* carried = CarriedBy(a_npc);
			for (std::uint32_t i = 0; carried && i < carried->numContainerObjects; ++i) {
				const auto* entry = carried->containerObjects[i];
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
		// shopkeepers and children, and ranking them squeezed everyone who
		// matters into the top half: a raider and a farmer landed 0.01 apart.
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
		// a form ID starting FF, and its template pointer points at that list,
		// not at a record. But the copy has its outfit and inventory filled in,
		// so it is measured on the spot from what Load worked out and lands on
		// the same scale.
		} else if (InPowerArmor(a_npc)) {
			care = 1.0F;
		} else if (const auto armor = ArmorIssued(a_npc)) {
			care = *armor > 0.0F ? PositionIn(OnARung(*armor)) : 0.0F;
		}

		// A unique, essential or protected character gets a boost, since
		// someone the game treats as special has their gear looked after. An
		// ordinary raider is none of these and a raider boss is.
		using Flag = RE::ACTOR_BASE_DATA::Flag;
		if (a_npc.actorData.actorBaseFlags.any(Flag::kUnique, Flag::kEssential, Flag::kProtected)) {
			care = care == UNMEASURED ? NAMED_CARE : std::lerp(care, 1.0F, NAMED_LIFT);
		}

		return care;
	}
}
