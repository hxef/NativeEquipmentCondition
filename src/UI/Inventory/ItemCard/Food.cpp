#include "UI/Inventory/ItemCard/Cards.h"

#include "Condition/Condition.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"

#include <cstdint>
#include <optional>

namespace ItemCard
{
	std::optional<std::uint32_t> FoodTenths(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack)
	{
		const auto freshness = a_item.object ? FoodSpoil::FreshnessOf(*a_item.object, a_stack ? a_stack->extra.get() : nullptr) : std::nullopt;

		// Rounded as a worn item is, so only spoiled food reads 0. Freshness
		// has no top cap, so food a moment from fresh reads 100.
		return freshness ? std::optional{ Condition::CardTenths(*freshness, false) } : std::nullopt;
	}

	bool InFridge(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack)
	{
		return a_stack && a_item.object && FoodSpoil::FridgeIconOn() && FoodSpoil::Spoils(*a_item.object) && FoodSpoil::InOpenFridge(a_stack->extra.get());
	}
}
