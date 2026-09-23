#include "Condition/Materials.h"

#include <algorithm>

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

		// Which recipe builds what: mods, and the weapons and armor with a
		// scrap recipe of their own. The recipe is kept, not what it asks for,
		// so the components are read fresh. Filled by Load and emptied by
		// Unload while nothing reads it, so no lock. A form built by 2 recipes
		// keeps the first, as the engine's own search does.
		std::unordered_map<const RE::TESForm*, const RE::BGSConstructibleObject*> g_recipes;

		// Vanilla's own median, used until Load has measured this load order.
		constexpr float DEFAULT_QUALITY = 20.0F;

		// The limits of the scale, a little wider than vanilla's wood at 2 and
		// nuclear material at 50, so a plugin's component priced at 0 or at
		// 1000 cannot stop wear entirely or make it absurdly fast.
		constexpr float LOWEST_QUALITY = 2.0F;
		constexpr float HIGHEST_QUALITY = 50.0F;

		float g_referenceQuality = DEFAULT_QUALITY;

		// The worth of everything one recipe asks for and how many units that
		// is, kept apart so a weapon's recipes are added up before dividing. An
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
				// A recipe can also ask for a finished item, priced in caps,
				// which says nothing about what it is made of.
				const auto found = required.first ? g_pricedBy.find(required.first) : g_pricedBy.end();
				const auto count = required.second.i;
				if (found == g_pricedBy.end() || count == 0) {
					continue;
				}

				const auto worth = found->second->value;
				if (worth <= 0) {
					continue;
				}
				parts.worth += static_cast<std::uint32_t>(worth) * count;
				parts.units += count;
			}
			return parts;
		}

		// What a recipe can build that matters here: mods, and weapons or
		// armor with a scrap recipe of their own.
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

		// The recipes an item is built from: its own scrap recipe, then the
		// recipe behind every enabled mod, the same 2 in the same order as the
		// engine's scrapping code. Mods no recipe builds are skipped, which is
		// what legendary effects are.
		template <class F>
		void ForEachRecipe(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra, F a_each)
		{
			const auto own = g_recipes.find(&a_object);
			if (own != g_recipes.end()) {
				a_each(*own->second);
			}

			// An item with no mods comes back from a save with no mod list, a
			// dress for example, and GetIndexData reads the list without
			// checking.
			const auto* mods = a_extra ? a_extra->GetByType<RE::BGSObjectInstanceExtra>() : nullptr;
			if (!mods || !mods->values) {
				return;
			}

			for (const auto& entry : mods->GetIndexData()) {
				if (entry.disabled) {
					continue;
				}

				const auto* mod = RE::TESForm::GetFormByID<RE::BGSMod::Attachment::Mod>(entry.objectID);
				const auto  found = mod ? g_recipes.find(mod) : g_recipes.end();
				if (found != g_recipes.end()) {
					a_each(*found->second);
				}
			}
		}
	}

	float ReferenceQuality()
	{
		return g_referenceQuality;
	}

	void Unload()
	{
		g_pricedBy.clear();
		g_recipes.clear();
		g_referenceQuality = DEFAULT_QUALITY;
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
			// list is how weapons get a recipe at all: a few scrap recipes each
			// name a list, so every knife scraps into the same 2 steel.
			if (built->Is(RE::ENUM_FORM_ID::kFLST)) {
				for (const auto* listed : static_cast<const RE::BGSListForm*>(built)->arrayOfForms) {
					remember(listed, *recipe);
				}
			} else {
				remember(built, *recipe);
			}
		}

		// The median recipe, not the average. A few recipes are gold and
		// nuclear material, most are steel and screws, and an average would be
		// dragged up by the few.
		std::vector<float> qualities;
		qualities.reserve(g_recipes.size());
		for (const auto& [built, recipe] : g_recipes) {
			const auto parts = PartsOf(*recipe);
			if (parts.units > 0) {
				qualities.push_back(static_cast<float>(parts.worth) / static_cast<float>(parts.units));
			}
		}

		if (!qualities.empty()) {
			const auto middleQuality = qualities.begin() + qualities.size() / 2;
			std::nth_element(qualities.begin(), middleQuality, qualities.end());
			g_referenceQuality = std::clamp(*middleQuality, LOWEST_QUALITY, HIGHEST_QUALITY);
		}

		REX::INFO("Found {:d} component spellings and what {:d} items and mods are built from. An ordinary one is worth {:.1f} a unit.",
			g_pricedBy.size(), g_recipes.size(), g_referenceQuality);
	}

	float Quality(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra)
	{
		Parts total;
		ForEachRecipe(a_object, a_extra, [&total](const RE::BGSConstructibleObject& a_recipe) {
			const auto parts = PartsOf(a_recipe);
			total.worth += parts.worth;
			total.units += parts.units;
		});

		if (total.units == 0) {
			return g_referenceQuality;
		}
		return std::clamp(static_cast<float>(total.worth) / static_cast<float>(total.units),
			LOWEST_QUALITY, HIGHEST_QUALITY);
	}

	std::vector<Line> BillOfLines(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra)
	{
		std::vector<Line> bill;
		ForEachRecipe(a_object, a_extra, [&bill](const RE::BGSConstructibleObject& a_recipe) {
			if (!a_recipe.requiredItems) {
				return;
			}

			for (const auto& required : *a_recipe.requiredItems) {
				// The same 2 reasons PartsOf passes a line over.
				const auto found = required.first ? g_pricedBy.find(required.first) : g_pricedBy.end();
				const auto count = required.second.i;
				if (found == g_pricedBy.end() || count == 0) {
					continue;
				}

				// One recipe naming the same component twice is merged into one
				// line, since nothing later could tell the 2 apart.
				const auto* component = found->second;
				const auto  already = std::find_if(bill.begin(), bill.end(),
					 [&a_recipe, component](const Line& a_line) {
						 return a_line.recipe == &a_recipe && a_line.component == component;
					 });
				if (already != bill.end()) {
					already->count += count;
				} else {
					bill.push_back({ &a_recipe, component, static_cast<std::uint32_t>(count) });
				}
			}
		});
		return bill;
	}

	std::vector<Part> BillOfParts(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra)
	{
		return BillOfParts(BillOfLines(a_object, a_extra));
	}

	std::vector<Part> BillOfParts(std::span<const Line> a_bill)
	{
		// A weapon's scrap recipe and its mods often name the same steel, and
		// the bill lists it once.
		std::vector<Part> bill;
		for (const auto& line : a_bill) {
			const auto already = std::find_if(bill.begin(), bill.end(),
				[&line](const Part& a_part) { return a_part.component == line.component; });
			if (already != bill.end()) {
				already->count += line.count;
			} else {
				bill.push_back({ line.component, line.count });
			}
		}
		return bill;
	}

	std::vector<const RE::BGSComponent*> Order(std::span<const Part> a_bill, std::uint32_t a_units)
	{
		// Where 1 unit of a component falls in the order. A component with a
		// count of n places its units at 0.5/n, 1.5/n and so on, so a component
		// with more units comes up sooner and more often.
		struct Seat
		{
			float                   at;
			std::uint32_t           count;
			std::uint32_t           id;
			const RE::BGSComponent* component;

			[[nodiscard]] bool operator<(const Seat& a_other) const
			{
				if (at != a_other.at) {
					return at < a_other.at;
				}
				if (count != a_other.count) {
					return count > a_other.count;
				}
				return id < a_other.id;
			}
		};

		// Every component gets as many places as a_units asks for, since a
		// weapon built from a single component takes them all.
		std::vector<Seat> seats;
		seats.reserve(a_bill.size() * a_units);
		for (const auto& part : a_bill) {
			if (!part.component || part.count == 0) {
				continue;
			}

			const auto count = static_cast<float>(part.count);
			for (std::uint32_t seat = 1; seat <= a_units; seat++) {
				seats.push_back({ (static_cast<float>(seat) - 0.5F) / count, part.count,
					part.component->formID, part.component });
			}
		}

		std::sort(seats.begin(), seats.end());
		seats.resize(std::min<std::size_t>(seats.size(), a_units));

		std::vector<const RE::BGSComponent*> out;
		out.reserve(seats.size());
		for (const auto& seat : seats) {
			out.push_back(seat.component);
		}
		return out;
	}
}
