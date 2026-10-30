#include "UI/Inventory/ItemCard/Cards.h"

#include "Condition/Condition.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"

#include <optional>

namespace ItemCard
{
	std::optional<double> FoodPercent(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack)
	{
		const auto freshness = a_item.object ? FoodSpoil::FreshnessOf(*a_item.object, a_stack ? a_stack->extra.get() : nullptr) : std::nullopt;

		// Rounded as a worn item is, so only spoiled food reads 0.
		return freshness ? std::optional{ Condition::WholePercent(*freshness) } : std::nullopt;
	}

	bool InFridge(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack)
	{
		return a_stack && a_item.object && FoodSpoil::FridgeIconOn() && FoodSpoil::Spoils(*a_item.object) && FoodSpoil::InOpenFridge(a_stack->extra.get());
	}
}
