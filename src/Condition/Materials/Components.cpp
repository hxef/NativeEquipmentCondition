#include "Condition/Materials/Index.h"

#include <algorithm>
#include <format>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Materials
{
	namespace
	{
		// Whether a_item's own Components list names a_component. The game
		// counts a junk item as the components that list names.
		bool Lists(const RE::TESObjectMISC& a_item, const RE::BGSComponent& a_component)
		{
			// The game may not check what form a Scrap Item field names, so a
			// bad plugin could put another form there.
			return a_item.Is(RE::ENUM_FORM_ID::kMISC) && a_item.componentData &&
			       std::ranges::any_of(*a_item.componentData, [&a_component](const auto& a_entry) {
					   return a_entry.first == &a_component;
				   });
		}
	}

	const RE::BGSComponent* PricedBy(const Index& a_index, const RE::TESForm* a_form)
	{
		const auto& pricedBy = a_index.pricedBy;
		const auto  found = a_form ? pricedBy.find(a_form) : pricedBy.end();
		return found == pricedBy.end() ? nullptr : found->second;
	}

	bool AsksForParts(const Index& a_index, const RE::BGSConstructibleObject& a_recipe)
	{
		if (!a_recipe.requiredItems) {
			return false;
		}
		for (const auto& required : *a_recipe.requiredItems) {
			if (PricedBy(a_index, required.first) && required.second.i != 0) {
				return true;
			}
		}
		return false;
	}

	std::string Spell(const Index& a_index, const RE::BGSConstructibleObject* a_recipe)
	{
		std::string out;
		if (a_recipe && a_recipe->requiredItems) {
			for (const auto& required : *a_recipe->requiredItems) {
				const auto* component = PricedBy(a_index, required.first);
				if (!component || required.second.i == 0) {
					continue;
				}
				out += std::format("{:s}{:d} {:s}", out.empty() ? "" : " and ", required.second.i,
					RE::TESFullName::GetFullName(*component));
			}
		}
		return out.empty() ? std::string{ "nothing" } : out;
	}

	void LoadComponents(Index& a_index, bool a_again)
	{
		const auto& components = g_dataHandler->GetFormArray<RE::BGSComponent>();

		// A mod's recipe asks for the component itself, steel. A scrap recipe
		// asks for the scrap item instead, c_Steel_scrap, a separate form with
		// a caps price and no worth. Both map to the component, and the
		// component is kept, not its worth, so the worth is read fresh each
		// time.
		auto& pricedBy = a_index.pricedBy;

		// A scrap item goes first to a component its own Components list
		// names. A plugin can add Packed Steel with the junk Steel as its scrap
		// item, and if Packed Steel took it, every 2 Steel of a scrap recipe
		// would cost 2 Packed Steel, which only a package of 100 holds. A
		// component priced at nothing is indexed too, since its worth is read
		// later.
		for (const auto* component : components) {
			if (!component) {
				continue;
			}
			pricedBy.try_emplace(component, component);
			if (component->scrapItem && Lists(*component->scrapItem, *component)) {
				pricedBy.try_emplace(component->scrapItem, component);
			}
		}

		// Then a scrap item no list claimed goes to the first component naming
		// it. A component that finds its scrap item taken is passed over, and
		// those scrap items are counted for the log.
		std::unordered_set<const RE::TESObjectMISC*> shared;
		const RE::BGSComponent* passedOver = nullptr;
		for (const auto* component : components) {
			if (!component || !component->scrapItem) {
				continue;
			}
			const auto [at, added] = pricedBy.try_emplace(component->scrapItem, component);
			if (!added && at->second != component) {
				shared.insert(component->scrapItem);
				if (!passedOver) {
					passedOver = component;
				}
			}
		}

		if (passedOver && !a_again) {
			const auto unlisted = std::ranges::count_if(shared, [&pricedBy](const auto* a_item) {
				return !Lists(*a_item, *pricedBy.at(a_item));
			});
			const auto* example = passedOver->scrapItem;
			REX::INFO("Junk items named as the scrap item of more than 1 component: {:d}. Each prices as the component its own Components list names, or as the first naming it where the list names none of them, which {:d} do. The junk {:s}, for one, prices as {:s} and not as {:s}.",
				shared.size(), unlisted, RE::TESFullName::GetFullName(*example), RE::TESFullName::GetFullName(*pricedBy.at(example)),
				RE::TESFullName::GetFullName(*passedOver));
		}
	}
}
