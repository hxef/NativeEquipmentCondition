#include "UI/Repair/SelectedItem.h"

#include "Condition/Condition.h"

namespace SelectedItem
{
	std::string Item::Name() const
	{
		if (item) {
			const auto* shown = item->GetDisplayFullName(extra);
			if (shown && *shown != '\0') {
				return shown;
			}
		}
		return object ? std::string{ RE::TESFullName::GetFullName(*object) } : std::string{ "nothing" };
	}

	Item Read(const RE::InventoryUserUIInterfaceEntry& a_entry)
	{
		Item        out;
		auto*       inventory = RE::BGSInventoryInterface::GetSingleton();
		const auto* item = inventory ? inventory->RequestInventoryItem(a_entry.invHandle.id) : nullptr;
		if (!item || !item->object || !Condition::WearsOut(*item->object)) {
			return out;
		}

		// A row can stand for several stacks, and the first is the one the
		// game takes the row's name from.
		const auto  stack = a_entry.stackIndex.size() > 0 ? static_cast<std::uint32_t>(a_entry.stackIndex[0]) : 0U;
		const auto* held = item->GetStackByID(stack);
		const auto  percent = Condition::Percent(*item, held);

		out.item = item;
		out.object = item->object;
		out.extra = held ? held->extra.get() : nullptr;
		out.handle = a_entry.invHandle.id;
		out.stack = stack;
		out.count = held ? held->GetCount() : 0U;
		out.percent = percent ? static_cast<std::uint32_t>(*percent) : Repair::FULL;
		return out;
	}
}
