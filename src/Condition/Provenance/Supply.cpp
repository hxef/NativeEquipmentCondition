#include "Condition/Provenance/Supply.h"

#include "Condition/Provenance/Rank.h"

namespace Provenance
{
	namespace
	{
		// -------------------------------------------------------------------
		// Walking a leveled list
		// -------------------------------------------------------------------

		// What a list gives out, and whether the walk had to give up somewhere.
		struct Walk
		{
			Haul haul;
			bool gaveUp{ false };
		};

		using Hauls = std::unordered_map<const RE::TESLevItem*, Haul>;
		using Open = std::unordered_set<const RE::TESLevItem*>;

		// The lists whose answer came out of a walk that could not finish.
		using Spoiled = std::unordered_set<const RE::TESLevItem*>;

		// Everything one walk needs to carry.
		struct Notes
		{
			Hauls   hauls;
			Open    open;
			Spoiled spoiled;
		};

		Walk HaulOf(const RE::TESLevItem& a_list, Notes& a_notes, std::size_t a_depth = 0)
		{
			// Every list is remembered after its first visit, finished or not,
			// which keeps the walk linear. Not remembering an unfinished answer
			// would walk a bad list again for every path into it, and a chain
			// where each list names the next twice would take 2 to the power of
			// its length and hang the loading screen. The answers that cannot
			// be trusted are marked, and MeasureSupply leaves those off the
			// scale, so a bad list does not spread.
			if (const auto found = a_notes.hauls.find(&a_list); found != a_notes.hauls.end()) {
				return { found->second, a_notes.spoiled.contains(&a_list) };
			}

			// A list that contains itself, and a chain too deep to be real, are
			// walks that cannot finish. Neither is remembered here, since
			// neither has been visited yet.
			if (a_depth >= DEEPEST || !a_notes.open.insert(&a_list).second) {
				return { {}, true };
			}

			Walk walk;

			// Kept separate, since what the list is worth in armor depends on
			// how it gives out entries, see below.
			float       armorSum = 0.0F;
			std::size_t armorEntries = 0;

			const auto entries = static_cast<std::size_t>(a_list.baseListCount);
			for (std::size_t i = 0; a_list.leveledLists && i < entries; ++i) {
				// What a script's AddForm put in, which the save brings back,
				// is left out, so the list ranks as the plugins and DLL mods
				// made it.
				const auto& entry = a_list.leveledLists[i];
				if (!entry.form || a_list.IsScriptAdded(&entry)) {
					continue;
				}
				const auto* form = entry.form;

				switch (form->GetFormType()) {
					case RE::ENUM_FORM_ID::kWEAP:
						// Read from the record and not from a copy, since the
						// mods a spawned gun gets are attached afterwards.
						walk.haul.weaponWorth += static_cast<const RE::TESObjectWEAP*>(form)->weaponData.value;
						++walk.haul.weapons;
						break;

					case RE::ENUM_FORM_ID::kARMO:
						{
							const auto& armor = *static_cast<const RE::TESObjectARMO*>(form);
							armorSum += static_cast<float>(armor.armorData.rating);
							++armorEntries;
							walk.haul.armorValue += armor.armorData.value;
							++walk.haul.armors;
							break;
						}

					case RE::ENUM_FORM_ID::kLVLI:
						{
							const auto nested = HaulOf(static_cast<const RE::TESLevItem&>(*form),
								a_notes, a_depth + 1);
							walk.haul.weaponWorth += nested.haul.weaponWorth;
							walk.haul.weapons += nested.haul.weapons;
							walk.haul.armorValue += nested.haul.armorValue;
							walk.haul.armors += nested.haul.armors;

							// A list inside a list counts as one entry worth
							// whatever it gives out.
							if (nested.haul.armorWorth > 0.0F) {
								armorSum += nested.haul.armorWorth;
								++armorEntries;
							}

							// One unfinished branch makes the whole answer
							// unfinished.
							walk.gaveUp = walk.gaveUp || nested.gaveUp;
							break;
						}

					default:
						break;
				}
			}

			// What this list is worth in armor. A list normally gives out one
			// entry at random, so it is worth the average. A list with the use
			// all flag gives out every entry, which is how a faction gives a
			// whole suit from one line, so it is worth the total. Averaging
			// those would count only 20% of the combat armor a gunner wears.
			// maxUseAllCount caps how many are kept and the engine drops the
			// rest, so a capped list is worth its share.
			if (armorEntries > 0) {
				const auto cap = static_cast<std::size_t>(a_list.maxUseAllCount);
				if (!a_list.GetUseAll()) {
					walk.haul.armorWorth = armorSum / static_cast<float>(armorEntries);
				} else if (cap > 0 && armorEntries > cap) {
					walk.haul.armorWorth = armorSum * static_cast<float>(cap) / static_cast<float>(armorEntries);
				} else {
					walk.haul.armorWorth = armorSum;
				}
			}

			a_notes.open.erase(&a_list);
			a_notes.hauls.emplace(&a_list, walk.haul);
			if (walk.gaveUp) {
				a_notes.spoiled.insert(&a_list);
			}
			return walk;
		}
	}

	SupplyCount MeasureSupply(Supply& a_out)
	{
		Notes notes;
		std::size_t spoiled = 0;

		// Every leveled list that can put a weapon in someone's hands, and
		// every one that can put armor on them, each on a scale of its own.
		// Lists giving out ammunition and junk are left out, so they do not
		// crowd either scale.
		std::vector<std::pair<RE::TESFormID, float>> pipelines;
		std::vector<std::pair<RE::TESFormID, float>> armorPipelines;
		for (const auto* list : g_dataHandler->GetFormArray<RE::TESLevItem>()) {
			if (!list) {
				continue;
			}

			// A list whose walk could not finish is left off the scale. Its
			// weapons fall back to the plain middle.
			const auto walk = HaulOf(*list, notes);
			if (walk.gaveUp) {
				a_out.ruined.insert(list);
				++spoiled;
				continue;
			}

			// Kept for the care half, which reads it back rather than walking
			// again.
			a_out.hauls.emplace(list, walk.haul);

			if (walk.haul.weapons > 0) {
				pipelines.emplace_back(list->formID,
					OnARung(static_cast<float>(walk.haul.weaponWorth) / static_cast<float>(walk.haul.weapons)));
			}
			if (walk.haul.armors > 0) {
				armorPipelines.emplace_back(list->formID,
					OnARung(static_cast<float>(walk.haul.armorValue) / static_cast<float>(walk.haul.armors)));
			}
		}
		a_out.weapons = PositionsOf(pipelines);
		a_out.armor = PositionsOf(armorPipelines);

		return { a_out.weapons.size(), a_out.armor.size(), spoiled };
	}

	std::optional<float> SupplyOf(const Supply& a_supply, RE::TESFormID a_list, Condition::Kind a_kind)
	{
		const auto& supply = a_kind == Condition::Kind::kArmor ? a_supply.armor : a_supply.weapons;
		const auto  found = supply.find(a_list);
		return found != supply.end() ? std::optional{ found->second } : std::nullopt;
	}

	std::optional<float> ArmorFrom(const Supply& a_supply, const RE::TESForm& a_form)
	{
		if (a_form.Is(RE::ENUM_FORM_ID::kARMO)) {
			return static_cast<float>(static_cast<const RE::TESObjectARMO&>(a_form).armorData.rating);
		}

		if (a_form.Is(RE::ENUM_FORM_ID::kLVLI)) {
			const auto* list = static_cast<const RE::TESLevItem*>(&a_form);
			if (a_supply.ruined.contains(list)) {
				return std::nullopt;
			}

			const auto found = a_supply.hauls.find(list);
			return found != a_supply.hauls.end() ? found->second.armorWorth : 0.0F;
		}

		return 0.0F;
	}
}
