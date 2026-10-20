#include "Condition/Materials/Materials.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/Materials/Index.h"
#include "Core/Snapshot.h"

#include <algorithm>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace Materials
{
	namespace
	{
		// The index wear and the bench read, see Index.h. A recipe is kept,
		// not what it asks for, so the components are read fresh.
		Snapshot<Index> g_index;

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

		// The median of a_kind in a_index, or vanilla's with none.
		float MedianOf(const Index* a_index, Condition::Kind a_kind)
		{
			if (a_kind == Condition::Kind::kArmor) {
				return a_index ? a_index->armorQuality : DEFAULT_ARMOR_QUALITY;
			}
			return a_index ? a_index->weaponQuality : DEFAULT_WEAPON_QUALITY;
		}

		// The worth of everything one recipe asks for and how many units that
		// is, kept apart so an item's recipes are added up before dividing. An
		// average of averages would let a cheap sight count as much as a
		// receiver 10 times its size.
		struct Parts
		{
			std::uint32_t worth{ 0 };
			std::uint32_t units{ 0 };
		};

		Parts PartsOf(const Index& a_index, const RE::BGSConstructibleObject& a_recipe)
		{
			Parts parts;
			if (!a_recipe.requiredItems) {
				return parts;
			}

			for (const auto& required : *a_recipe.requiredItems) {
				const auto* component = PricedBy(a_index, required.first);
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
		std::pair<float, float> Medians(const Index& a_index)
		{
			std::vector<float> weapons;
			std::vector<float> armor;
			const auto         add = [&a_index, &weapons, &armor](const auto& a_recipes) {
				for (const auto& [built, recipe] : a_recipes) {
					const auto kind = CountsToward(*built);
					const auto parts = PartsOf(a_index, *recipe);
					if (!kind || parts.units == 0) {
						continue;
					}
					(*kind == Condition::Kind::kArmor ? armor : weapons)
						.push_back(static_cast<float>(parts.worth) / static_cast<float>(parts.units));
				}
			};
			add(a_index.mods);
			// The lists' own forms, see ReadScrapLists.
			add(a_index.scrapping.recipes);
			return { Middle(weapons, DEFAULT_WEAPON_QUALITY), Middle(armor, DEFAULT_ARMOR_QUALITY) };
		}

		// a_quality kept inside the limits of the scale for an item of a_kind.
		float Limited(const Index& a_index, float a_quality, Condition::Kind a_kind)
		{
			const auto vanilla = MedianOf(nullptr, a_kind);
			const auto scale = MedianOf(&a_index, a_kind) / vanilla;
			return std::clamp(a_quality, LOWEST_QUALITY * std::min(scale, 1.0F), HIGHEST_QUALITY * std::max(scale, 1.0F));
		}

		// Whether a recipe that asks for a component builds a_form: a mod's
		// recipe, or a weapon's scrap recipe.
		bool BuiltPriced(const Index& a_index, const RE::TESForm* a_form)
		{
			const auto& recipes = a_form && a_form->Is(RE::ENUM_FORM_ID::kOMOD) ? a_index.mods : a_index.scrapping.recipes;
			const auto  found = a_form ? recipes.find(a_form) : recipes.end();
			return found != recipes.end() && AsksForParts(a_index, *found->second);
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

		// Reads every component, recipe and scrap list into a new index.
		// a_again for a measure after Load, which leaves Load's lines out of
		// NEC.log.
		std::shared_ptr<Index> Measure(bool a_again)
		{
			auto index = std::make_shared<Index>();
			LoadComponents(*index, a_again);

			const auto remember = [&index](const RE::TESForm* a_built, const RE::BGSConstructibleObject& a_recipe) {
				if (a_built && a_built->Is(RE::ENUM_FORM_ID::kOMOD)) {
					index->mods.emplace(a_built, &a_recipe);
				}
			};

			// A mod is built by the first recipe naming it or a form list holding
			// it, as the engine searches in
			// BGSConstructibleObject::FindRecipeForCreatedForm. A recipe naming a
			// weapon or a wearable itself crafts it and counts for nothing here.
			// A list counts by its own forms, without what a script added, see
			// ReadScrapLists.
			for (auto* recipe : g_dataHandler->GetFormArray<RE::BGSConstructibleObject>()) {
				const auto* built = recipe ? recipe->createdItem : nullptr;
				if (!built) {
					continue;
				}
				if (built->Is(RE::ENUM_FORM_ID::kFLST)) {
					for (const auto* listed : static_cast<const RE::BGSListForm*>(built)->GetOwnForms()) {
						remember(listed, *recipe);
					}
				} else {
					remember(built, *recipe);
				}
			}

			LoadScrapLists(*index, a_again);
			std::tie(index->weaponQuality, index->armorQuality) = Medians(*index);
			return index;
		}
	}

	void ForEachRecipe(const Index& a_index, const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra,
		const std::function<void(const RE::BGSConstructibleObject&)>& a_each)
	{
		// Whether any recipe so far asked for a component, which decides
		// whether a weapon borrows.
		bool       priced = false;
		const auto each = [&a_index, &a_each, &priced](const RE::BGSConstructibleObject& a_recipe) {
			priced = priced || AsksForParts(a_index, a_recipe);
			a_each(a_recipe);
		};

		// The item's own scrap recipe from the last reading of the lists, see
		// ReadScrapLists.
		const auto& scrapping = a_index.scrapping;
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
				const auto  found = mod ? a_index.mods.find(mod) : a_index.mods.end();
				if (found != a_index.mods.end()) {
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

	std::shared_ptr<const Index> Current()
	{
		return g_index.Get();
	}

	float ReferenceQuality(Condition::Kind a_kind)
	{
		return MedianOf(Current().get(), a_kind);
	}

	std::vector<std::string> MeasureAgain()
	{
		// Nothing to measure again between Unload and Load, while a full
		// reset deletes the forms.
		const auto was = Current();
		if (!was) {
			return {};
		}
		auto now = Measure(true);

		const auto said = std::format("{:.1f}", now->weaponQuality);
		const auto saidBefore = std::format("{:.1f}", was->weaponQuality);
		if (said != saidBefore) {
			REX::INFO("An ordinary weapon is now worth {:s} a unit instead of {:s}, as a mod changed components or recipes after game data loaded.",
				said, saidBefore);
		}
		const auto armorSaid = std::format("{:.1f}", now->armorQuality);
		const auto armorSaidBefore = std::format("{:.1f}", was->armorQuality);
		if (armorSaid != armorSaidBefore) {
			REX::INFO("An ordinary piece of armor is now worth {:s} a unit instead of {:s}, as a mod changed components or recipes after game data loaded.",
				armorSaid, armorSaidBefore);
		}

		std::vector<std::string> moved;
		const auto built = ChangedKeys(was->mods, now->mods) + ChangedKeys(was->scrapping.recipes, now->scrapping.recipes);
		if (built > 0) {
			moved.push_back(std::format("what {:d} items and mods are built from", built));
		}
		if (const auto spellings = ChangedKeys(was->pricedBy, now->pricedBy); spellings > 0) {
			moved.push_back(std::format("{:d} component spellings", spellings));
		}
		const auto& before = was->scrapping;
		const auto& after = now->scrapping;
		if (before.borrowedClothing != after.borrowedClothing || before.borrowedArmor != after.borrowedArmor ||
			before.borrowedWeapon != after.borrowedWeapon) {
			moved.emplace_back("what an item with nothing of its own borrows");
		}
		g_index.Publish(std::move(now));
		return moved;
	}

	void Unload()
	{
		g_index.Publish(nullptr);
		UnloadScrapLists();
	}

	void Load()
	{
		Unload();
		if (!g_dataHandler) {
			return;
		}

		auto index = Measure(false);

		// The weapons that borrow, for the log: no priced recipe of their own
		// and no mod with a priced recipe in any of the mod sets their object
		// template can give them. Buffer 0 of each set lists its mods.
		std::size_t borrowing = 0;
		for (const auto* weapon : g_dataHandler->GetFormArray<RE::TESObjectWEAP>()) {
			if (!weapon || !Condition::WearsOut(*weapon) || BuiltPriced(*index, weapon)) {
				continue;
			}
			const auto priced = std::ranges::any_of(weapon->objectTemplate.items, [&index](const RE::BGSMod::Template::Item* a_item) {
				return a_item && std::ranges::any_of(a_item->GetBuffer<RE::BGSMod::Attachment::Instance>(0),
									 [&index](const RE::BGSMod::Attachment::Instance& a_mod) { return BuiltPriced(*index, a_mod.mod); });
			});
			if (!priced) {
				borrowing++;
			}
		}

		const auto& scrapping = index->scrapping;
		REX::INFO("Found {:d} component spellings and what {:d} items and mods are built from. An ordinary weapon is worth {:.1f} a unit and an ordinary piece of armor {:.1f}. Clothing with no scrap recipe of its own borrows {:s}, armor {:s}, and a weapon with nothing priced {:s}, which {:d} of this load order's weapons do.",
			index->pricedBy.size(), index->mods.size() + scrapping.recipes.size(), index->weaponQuality, index->armorQuality,
			Spell(*index, scrapping.borrowedClothing), Spell(*index, scrapping.borrowedArmor), Spell(*index, scrapping.borrowedWeapon), borrowing);
		REX::INFO("Found {:d} scrap recipes the way the workbench finds them.", index->lists.size());
		g_index.Publish(std::move(index));
	}

	float Quality(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra)
	{
		// 1 index for the whole call, so the recipes and the median agree.
		const auto kind = Condition::KindOf(a_object);
		const auto index = Current();
		if (!index) {
			return MedianOf(nullptr, kind);
		}

		Parts total;
		ForEachRecipe(*index, a_object, a_extra, [&index, &total](const RE::BGSConstructibleObject& a_recipe) {
			const auto parts = PartsOf(*index, a_recipe);
			total.worth += parts.worth;
			total.units += parts.units;
		});

		if (total.units == 0) {
			return MedianOf(index.get(), kind);
		}
		return Limited(*index, static_cast<float>(total.worth) / static_cast<float>(total.units), kind);
	}
}
