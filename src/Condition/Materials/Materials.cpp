#include "Condition/Materials/Materials.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/Materials/Index.h"

#include <algorithm>
#include <format>
#include <optional>
#include <string>

namespace Materials
{
	namespace
	{
		// Which component prices a form. A mod's recipe asks for the component
		// itself, steel. A scrap recipe asks for the scrap item instead,
		// c_Steel_scrap, a separate form with a caps price and no worth. Both
		// map to the component here. The component is kept, not its worth, so
		// the worth is read fresh each time.
		std::unordered_map<const RE::TESForm*, const RE::BGSComponent*> g_pricedBy;

		// Which recipe builds what: mods, and the weapons and wearables with a
		// scrap recipe of their own. The recipe is kept, not what it asks for,
		// so the components are read fresh. Filled by Load and emptied by
		// Unload while nothing reads it, so no lock. A form built by 2 recipes
		// keeps the first, as the engine's own search does.
		std::unordered_map<const RE::TESForm*, const RE::BGSConstructibleObject*> g_recipes;

		// The scrap recipe a wearable with none of its own borrows, one for
		// clothing and one for armor, and the one a weapon with nothing priced
		// borrows, see Materials.h. Empty in a load order with no scrap recipe
		// for that kind at all.
		const RE::BGSConstructibleObject* g_borrowedClothing = nullptr;
		const RE::BGSConstructibleObject* g_borrowedArmor = nullptr;
		const RE::BGSConstructibleObject* g_borrowedWeapon = nullptr;

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

		float g_weaponQuality = DEFAULT_WEAPON_QUALITY;
		float g_armorQuality = DEFAULT_ARMOR_QUALITY;

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

		// What a recipe can build that matters here: mods, and weapons or
		// wearables with a scrap recipe of their own.
		bool WorthIndexing(const RE::TESForm& a_form)
		{
			switch (a_form.GetFormType()) {
				case RE::ENUM_FORM_ID::kOMOD:
				case RE::ENUM_FORM_ID::kWEAP:
				case RE::ENUM_FORM_ID::kARMO:
					return true;
				default:
					return false;
			}
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

		// a_quality kept inside the limits of the scale for an item of a_kind.
		float Limited(float a_quality, Condition::Kind a_kind)
		{
			const auto vanilla = a_kind == Condition::Kind::kArmor ? DEFAULT_ARMOR_QUALITY : DEFAULT_WEAPON_QUALITY;
			const auto scale = ReferenceQuality(a_kind) / vanilla;
			return std::clamp(a_quality, LOWEST_QUALITY * std::min(scale, 1.0F), HIGHEST_QUALITY * std::max(scale, 1.0F));
		}

		// Whether a recipe asks for any component. A line for a finished item
		// or for nothing at all does not count, as in the bill.
		bool AsksForParts(const RE::BGSConstructibleObject& a_recipe)
		{
			if (!a_recipe.requiredItems) {
				return false;
			}
			for (const auto& required : *a_recipe.requiredItems) {
				if (PricedBy(required.first) && required.second.i != 0) {
					return true;
				}
			}
			return false;
		}

		// Whether a recipe that asks for a component builds a_form.
		bool BuiltPriced(const RE::TESForm* a_form)
		{
			const auto found = a_form ? g_recipes.find(a_form) : g_recipes.end();
			return found != g_recipes.end() && AsksForParts(*found->second);
		}

		// The recipe most items of a kind scrap into, over every item that
		// takes part, has a scrap recipe of its own and a_ofKind keeps. Ties
		// go to the lower form ID, so the answer is the same on every load.
		template <class Item>
		const RE::BGSConstructibleObject* MostBorrowed(bool (*a_ofKind)(const Item&))
		{
			std::unordered_map<const RE::BGSConstructibleObject*, std::uint32_t> items;
			for (const auto* item : g_dataHandler->GetFormArray<Item>()) {
				if (!item || !Condition::WearsOut(*item) || !a_ofKind(*item)) {
					continue;
				}
				const auto own = g_recipes.find(item);
				if (own != g_recipes.end()) {
					items[own->second]++;
				}
			}

			const RE::BGSConstructibleObject* most = nullptr;
			std::uint32_t                     count = 0;
			for (const auto& [recipe, uses] : items) {
				if (uses > count || (uses == count && most && recipe->formID < most->formID)) {
					most = recipe;
					count = uses;
				}
			}
			return most;
		}

		// The recipe a wearable with none of its own borrows, or nothing for a
		// kind the load order has no scrap recipe for. Nothing for a weapon,
		// which borrows only once its mods are known, see ForEachRecipe.
		const RE::BGSConstructibleObject* Borrowed(const RE::TESBoundObject& a_object)
		{
			if (!a_object.Is(RE::ENUM_FORM_ID::kARMO)) {
				return nullptr;
			}
			const auto& armor = static_cast<const RE::TESObjectARMO&>(a_object);
			return ArmorWear::IsClothing(armor) ? g_borrowedClothing : g_borrowedArmor;
		}

		// What a recipe asks for, for the log: "2 Cloth", or "nothing" for no
		// recipe at all.
		std::string Spell(const RE::BGSConstructibleObject* a_recipe)
		{
			std::string out;
			if (a_recipe && a_recipe->requiredItems) {
				for (const auto& required : *a_recipe->requiredItems) {
					const auto* component = PricedBy(required.first);
					if (!component || required.second.i == 0) {
						continue;
					}
					out += std::format("{:s}{:d} {:s}", out.empty() ? "" : " and ", required.second.i,
						RE::TESFullName::GetFullName(*component));
				}
			}
			return out.empty() ? std::string{ "nothing" } : out;
		}
	}

	const RE::BGSComponent* PricedBy(const RE::TESForm* a_form)
	{
		const auto found = a_form ? g_pricedBy.find(a_form) : g_pricedBy.end();
		return found == g_pricedBy.end() ? nullptr : found->second;
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

		const auto own = g_recipes.find(&a_object);
		if (own != g_recipes.end()) {
			each(*own->second);
		} else if (const auto* borrowed = Borrowed(a_object)) {
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
				const auto  found = mod ? g_recipes.find(mod) : g_recipes.end();
				if (found != g_recipes.end()) {
					each(*found->second);
				}
			}
		}

		// A weapon borrows last, once its own recipe and its mods asked for
		// nothing.
		if (!priced && a_object.IsWeapon() && g_borrowedWeapon) {
			a_each(*g_borrowedWeapon);
		}
	}

	float ReferenceQuality(Condition::Kind a_kind)
	{
		return a_kind == Condition::Kind::kArmor ? g_armorQuality : g_weaponQuality;
	}

	void Unload()
	{
		g_pricedBy.clear();
		g_recipes.clear();
		g_borrowedClothing = nullptr;
		g_borrowedArmor = nullptr;
		g_borrowedWeapon = nullptr;
		g_weaponQuality = DEFAULT_WEAPON_QUALITY;
		g_armorQuality = DEFAULT_ARMOR_QUALITY;
	}

	void Load()
	{
		Unload();
		if (!g_dataHandler) {
			return;
		}

		// Both forms of every component, see g_pricedBy. One priced at nothing
		// is indexed too, since its worth is read later.
		for (const auto* component : g_dataHandler->GetFormArray<RE::BGSComponent>()) {
			if (!component) {
				continue;
			}
			g_pricedBy.insert_or_assign(component, component);
			if (component->scrapItem) {
				g_pricedBy.insert_or_assign(component->scrapItem, component);
			}
		}

		const auto remember = [](const RE::TESForm* a_built, const RE::BGSConstructibleObject& a_recipe) {
			if (a_built && WorthIndexing(*a_built)) {
				g_recipes.emplace(a_built, &a_recipe);
			}
		};

		for (auto* recipe : g_dataHandler->GetFormArray<RE::BGSConstructibleObject>()) {
			const auto* built = recipe ? recipe->createdItem : nullptr;
			if (!built) {
				continue;
			}

			// A recipe names the item it builds or a form list of items. The
			// list is how weapons and wearables get a scrap recipe at all: a
			// few recipes each name a list, so every knife scraps into the same
			// 2 steel and every suit into the same 2 cloth.
			if (built->Is(RE::ENUM_FORM_ID::kFLST)) {
				for (const auto* listed : static_cast<const RE::BGSListForm*>(built)->arrayOfForms) {
					remember(listed, *recipe);
				}
			} else {
				remember(built, *recipe);
			}
		}

		// The 2 medians, see ReferenceQuality in Materials.h.
		std::vector<float> weapons;
		std::vector<float> armor;
		for (const auto& [built, recipe] : g_recipes) {
			const auto kind = CountsToward(*built);
			const auto parts = PartsOf(*recipe);
			if (!kind || parts.units == 0) {
				continue;
			}
			(*kind == Condition::Kind::kArmor ? armor : weapons)
				.push_back(static_cast<float>(parts.worth) / static_cast<float>(parts.units));
		}
		g_weaponQuality = Middle(weapons, DEFAULT_WEAPON_QUALITY);
		g_armorQuality = Middle(armor, DEFAULT_ARMOR_QUALITY);

		g_borrowedClothing = MostBorrowed<RE::TESObjectARMO>([](const RE::TESObjectARMO& a_armor) { return ArmorWear::IsClothing(a_armor); });
		g_borrowedArmor = MostBorrowed<RE::TESObjectARMO>([](const RE::TESObjectARMO& a_armor) { return !ArmorWear::IsClothing(a_armor); });
		g_borrowedWeapon = MostBorrowed<RE::TESObjectWEAP>([](const RE::TESObjectWEAP&) { return true; });

		// The weapons that borrow, for the log: no priced recipe of their own
		// and no mod with a priced recipe in any of the mod sets their object
		// template can give them. Buffer 0 of each set lists its mods.
		std::size_t borrowing = 0;
		for (const auto* weapon : g_dataHandler->GetFormArray<RE::TESObjectWEAP>()) {
			if (!weapon || !Condition::WearsOut(*weapon) || BuiltPriced(weapon)) {
				continue;
			}
			const auto priced = std::ranges::any_of(weapon->objectTemplate.items, [](const RE::BGSMod::Template::Item* a_item) {
				return a_item && std::ranges::any_of(a_item->GetBuffer<RE::BGSMod::Attachment::Instance>(0),
									 [](const RE::BGSMod::Attachment::Instance& a_mod) { return BuiltPriced(a_mod.mod); });
			});
			if (!priced) {
				borrowing++;
			}
		}

		REX::INFO("Found {:d} component spellings and what {:d} items and mods are built from. An ordinary weapon is worth {:.1f} a unit and an ordinary piece of armor {:.1f}. Clothing with no scrap recipe of its own borrows {:s}, armor {:s}, and a weapon with nothing priced {:s}, which {:d} of this load order's weapons do.",
			g_pricedBy.size(), g_recipes.size(), g_weaponQuality, g_armorQuality, Spell(g_borrowedClothing), Spell(g_borrowedArmor), Spell(g_borrowedWeapon), borrowing);
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
