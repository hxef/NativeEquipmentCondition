#include "Condition/Materials/Materials.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/Materials/Index.h"

#include <algorithm>
#include <atomic>
#include <format>
#include <optional>
#include <string>
#include <utility>

namespace Materials
{
	namespace
	{
		// Which recipe builds each mod. The recipe is kept, not what it asks
		// for, so the components are read fresh. Filled by Load and emptied by
		// Unload while nothing reads it, so no lock. A mod built by 2 recipes
		// keeps the first, as the engine's own search does. Weapons and
		// wearables get their scrap recipe from Scrap.cpp.
		std::unordered_map<const RE::TESForm*, const RE::BGSConstructibleObject*> g_mods;

		// Vanilla's own medians, see ReferenceQuality in Materials.h, used
		// until Load has measured this load order, and the base the limits
		// below widen from.
		constexpr float DEFAULT_WEAPON_QUALITY = 19.0F;
		constexpr float DEFAULT_ARMOR_QUALITY = 4.0F;

		// The limits of the scale, vanilla's wood at 2 and nuclear material at
		// 50, so a plugin's component priced at 0 or at 1000 cannot stop wear
		// or make it absurdly fast. A median above or below vanilla's widens
		// them by the same factor, so an overhaul repricing every component
		// alike wears as vanilla does, and vanilla's own items stay inside.
		constexpr float LOWEST_QUALITY = 2.0F;
		constexpr float HIGHEST_QUALITY = 50.0F;

		// Measured by Load and MeasureAgain. Atomic, since wear reads them on
		// other threads.
		std::atomic<float> g_weaponQuality{ DEFAULT_WEAPON_QUALITY };
		std::atomic<float> g_armorQuality{ DEFAULT_ARMOR_QUALITY };

		// The worth of everything one recipe asks for and how many units that
		// is, kept apart so an item's recipes are added up before dividing. An
		// average of averages would let a cheap sight count as much as a
		// receiver 10 times its size.
		struct Parts
		{
			std::uint32_t worth{ 0 };
			std::uint32_t units{ 0 };
		};

		Parts PartsOf(const RE::BGSConstructibleObject& a_recipe)
		{
			Parts parts;
			if (!a_recipe.requiredItems) {
				return parts;
			}

			for (const auto& required : *a_recipe.requiredItems) {
				const auto* component = PricedBy(required.first);
				const auto  count = required.second.i;
				if (!component || count == 0) {
					continue;
				}

				const auto worth = component->value;
				if (worth <= 0) {
					continue;
				}
				parts.worth += static_cast<std::uint32_t>(worth) * count;
				parts.units += count;
			}
			return parts;
		}

		// Which median a recipe counts toward, see ReferenceQuality in
		// Materials.h: weapons and weapon mods toward the weapon's, wearables
		// toward armor's, and nothing for a wearable's mods or a robot's.
		std::optional<Condition::Kind> CountsToward(const RE::TESForm& a_built)
		{
			switch (a_built.GetFormType()) {
				case RE::ENUM_FORM_ID::kWEAP:
					return Condition::Kind::kWeapon;
				case RE::ENUM_FORM_ID::kARMO:
					return Condition::Kind::kArmor;
				case RE::ENUM_FORM_ID::kOMOD:
					{
						const auto& mod = static_cast<const RE::BGSMod::Attachment::Mod&>(a_built);
						if (mod.targetFormType.get() == RE::ENUM_FORM_ID::kWEAP) {
							return Condition::Kind::kWeapon;
						}
						return std::nullopt;
					}
				default:
					return std::nullopt;
			}
		}

		// The median of a_qualities, not the average. A few recipes are gold
		// and nuclear material, most are steel and screws, and an average would
		// be dragged up by the few. a_standing for an empty list.
		float Middle(std::vector<float>& a_qualities, float a_standing)
		{
			if (a_qualities.empty()) {
				return a_standing;
			}
			const auto middle = a_qualities.begin() + a_qualities.size() / 2;
			std::nth_element(a_qualities.begin(), middle, a_qualities.end());
			return *middle;
		}

		// The 2 medians, see ReferenceQuality in Materials.h, weapons first,
		// from what each component is worth now.
		std::pair<float, float> Medians()
		{
			std::vector<float> weapons;
			std::vector<float> armor;
			const auto         add = [&weapons, &armor](const auto& a_recipes) {
				for (const auto& [built, recipe] : a_recipes) {
					const auto kind = CountsToward(*built);
					const auto parts = PartsOf(*recipe);
					if (!kind || parts.units == 0) {
						continue;
					}
					(*kind == Condition::Kind::kArmor ? armor : weapons)
						.push_back(static_cast<float>(parts.worth) / static_cast<float>(parts.units));
				}
			};
			add(g_mods);
			// The lists as Load read them, see ReadScrapLists.
			add(Scrapped().recipes);
			return { Middle(weapons, DEFAULT_WEAPON_QUALITY), Middle(armor, DEFAULT_ARMOR_QUALITY) };
		}

		// a_quality kept inside the limits of the scale for an item of a_kind.
		float Limited(float a_quality, Condition::Kind a_kind)
		{
			const auto vanilla = a_kind == Condition::Kind::kArmor ? DEFAULT_ARMOR_QUALITY : DEFAULT_WEAPON_QUALITY;
			const auto scale = ReferenceQuality(a_kind) / vanilla;
			return std::clamp(a_quality, LOWEST_QUALITY * std::min(scale, 1.0F), HIGHEST_QUALITY * std::max(scale, 1.0F));
		}

		// Whether a recipe that asks for a component builds a_form: a mod's
		// recipe, or a weapon's scrap recipe.
		bool BuiltPriced(const RE::TESForm* a_form, const Scrapping& a_scrapping)
		{
			const auto& recipes = a_form && a_form->Is(RE::ENUM_FORM_ID::kOMOD) ? g_mods : a_scrapping.recipes;
			const auto  found = a_form ? recipes.find(a_form) : recipes.end();
			return found != recipes.end() && AsksForParts(*found->second);
		}

		// The recipe a wearable with none of its own borrows, or nothing for a
		// kind the load order has no scrap recipe for. Nothing for a weapon,
		// which borrows only once its mods are known, see ForEachRecipe.
		const RE::BGSConstructibleObject* Borrowed(const RE::TESBoundObject& a_object, const Scrapping& a_scrapping)
		{
			if (!a_object.Is(RE::ENUM_FORM_ID::kARMO)) {
				return nullptr;
			}
			const auto& armor = static_cast<const RE::TESObjectARMO&>(a_object);
			return ArmorWear::IsClothing(armor) ? a_scrapping.borrowedClothing : a_scrapping.borrowedArmor;
		}
	}

	void ForEachRecipe(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra,
		const std::function<void(const RE::BGSConstructibleObject&)>& a_each)
	{
		// Whether any recipe so far asked for a component, which decides
		// whether a weapon borrows.
		bool       priced = false;
		const auto each = [&a_each, &priced](const RE::BGSConstructibleObject& a_recipe) {
			priced = priced || AsksForParts(a_recipe);
			a_each(a_recipe);
		};

		// The item's own scrap recipe from the last reading of the lists, see
		// ReadScrapLists.
		const auto& scrapping = Scrapped();
		if (const auto* own = OwnRecipe(a_object)) {
			each(*own);
		} else if (const auto* borrowed = Borrowed(a_object, scrapping)) {
			each(*borrowed);
		}

		// An item with no mods comes back from a save with no mod list, a dress
		// for example, and GetIndexData reads the list without checking.
		const auto* mods = a_extra ? a_extra->GetByType<RE::BGSObjectInstanceExtra>() : nullptr;
		if (mods && mods->values) {
			// The legendary effect is left out. Vanilla builds it from no
			// recipe, but a plugin such as AWKCR gives it one, which would put
			// legendary parts in a repair and in the wear rate. The game names
			// only 1 legendary mod an item, so on an item a plugin gives 2 or
			// more, the rest still count. CommonLibF4 declares the lookup
			// without const.
			const auto* legendary = const_cast<RE::ExtraDataList*>(a_extra)->GetLegendaryMod();

			for (const auto& entry : mods->GetIndexData()) {
				if (entry.disabled) {
					continue;
				}

				const auto* mod = RE::TESForm::GetFormByID<RE::BGSMod::Attachment::Mod>(entry.objectID);
				if (mod && mod == legendary) {
					continue;
				}
				const auto  found = mod ? g_mods.find(mod) : g_mods.end();
				if (found != g_mods.end()) {
					each(*found->second);
				}
			}
		}

		// A weapon borrows last, once its own recipe and its mods asked for
		// nothing.
		if (!priced && a_object.IsWeapon() && scrapping.borrowedWeapon) {
			a_each(*scrapping.borrowedWeapon);
		}
	}

	float ReferenceQuality(Condition::Kind a_kind)
	{
		return a_kind == Condition::Kind::kArmor ? g_armorQuality.load() : g_weaponQuality.load();
	}

	void MeasureAgain()
	{
		const auto [weapons, armor] = Medians();
		const auto weaponsBefore = g_weaponQuality.exchange(weapons);
		const auto armorBefore = g_armorQuality.exchange(armor);

		const auto said = std::format("{:.1f}", weapons);
		const auto saidBefore = std::format("{:.1f}", weaponsBefore);
		if (said != saidBefore) {
			REX::INFO("An ordinary weapon is now worth {:s} a unit instead of {:s}, as a mod changed components or recipes after game data loaded.",
				said, saidBefore);
		}
		const auto armorSaid = std::format("{:.1f}", armor);
		const auto armorSaidBefore = std::format("{:.1f}", armorBefore);
		if (armorSaid != armorSaidBefore) {
			REX::INFO("An ordinary piece of armor is now worth {:s} a unit instead of {:s}, as a mod changed components or recipes after game data loaded.",
				armorSaid, armorSaidBefore);
		}
	}

	void Unload()
	{
		UnloadComponents();
		UnloadScrapLists();
		g_mods.clear();
		g_weaponQuality = DEFAULT_WEAPON_QUALITY;
		g_armorQuality = DEFAULT_ARMOR_QUALITY;
	}

	void Load()
	{
		Unload();
		if (!g_dataHandler) {
			return;
		}

		const auto spellings = LoadComponents();

		const auto remember = [](const RE::TESForm* a_built, const RE::BGSConstructibleObject& a_recipe) {
			if (a_built && a_built->Is(RE::ENUM_FORM_ID::kOMOD)) {
				g_mods.emplace(a_built, &a_recipe);
			}
		};

		// A mod is built by the first recipe naming it or a form list holding
		// it, as the engine searches in
		// BGSConstructibleObject::FindRecipeForCreatedForm. A recipe naming a
		// weapon or a wearable itself crafts it and counts for nothing here.
		for (auto* recipe : g_dataHandler->GetFormArray<RE::BGSConstructibleObject>()) {
			const auto* built = recipe ? recipe->createdItem : nullptr;
			if (!built) {
				continue;
			}
			if (built->Is(RE::ENUM_FORM_ID::kFLST)) {
				for (const auto* listed : static_cast<const RE::BGSListForm*>(built)->arrayOfForms) {
					remember(listed, *recipe);
				}
			} else {
				remember(built, *recipe);
			}
		}

		const auto  lists = LoadScrapLists();
		const auto& scrapping = Scrapped();

		const auto [weapons, armor] = Medians();
		g_weaponQuality = weapons;
		g_armorQuality = armor;

		// The weapons that borrow, for the log: no priced recipe of their own
		// and no mod with a priced recipe in any of the mod sets their object
		// template can give them. Buffer 0 of each set lists its mods.
		std::size_t borrowing = 0;
		for (const auto* weapon : g_dataHandler->GetFormArray<RE::TESObjectWEAP>()) {
			if (!weapon || !Condition::WearsOut(*weapon) || BuiltPriced(weapon, scrapping)) {
				continue;
			}
			const auto priced = std::ranges::any_of(weapon->objectTemplate.items, [&scrapping](const RE::BGSMod::Template::Item* a_item) {
				return a_item && std::ranges::any_of(a_item->GetBuffer<RE::BGSMod::Attachment::Instance>(0),
									 [&scrapping](const RE::BGSMod::Attachment::Instance& a_mod) { return BuiltPriced(a_mod.mod, scrapping); });
			});
			if (!priced) {
				borrowing++;
			}
		}

		REX::INFO("Found {:d} component spellings and what {:d} items and mods are built from. An ordinary weapon is worth {:.1f} a unit and an ordinary piece of armor {:.1f}. Clothing with no scrap recipe of its own borrows {:s}, armor {:s}, and a weapon with nothing priced {:s}, which {:d} of this load order's weapons do.",
			spellings, g_mods.size() + scrapping.recipes.size(), weapons, armor, Spell(scrapping.borrowedClothing), Spell(scrapping.borrowedArmor),
			Spell(scrapping.borrowedWeapon), borrowing);
		REX::INFO("Found {:d} scrap recipes the way the workbench finds them.", lists);
	}

	float Quality(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra)
	{
		Parts total;
		ForEachRecipe(a_object, a_extra, [&total](const RE::BGSConstructibleObject& a_recipe) {
			const auto parts = PartsOf(a_recipe);
			total.worth += parts.worth;
			total.units += parts.units;
		});

		const auto kind = Condition::KindOf(a_object);
		if (total.units == 0) {
			return ReferenceQuality(kind);
		}
		return Limited(static_cast<float>(total.worth) / static_cast<float>(total.units), kind);
	}
}
