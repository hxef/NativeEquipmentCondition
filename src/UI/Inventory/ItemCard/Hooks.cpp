#include "UI/Inventory/ItemCard/Cards.h"

#include "Condition/Condition.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/TraceLog.h"
#include "UI/Inventory/Pipboy.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <utility>

namespace ItemCard
{
	namespace
	{
		using CallPatch::AE;
		using CallPatch::CallSite;
		using CallPatch::OG;
		using Scaleform::GFx::Value;

		// The calls to InventoryUserUIUtils::PopulateItemCardInfo_Helper, which
		// builds the card for every menu except the Pip-Boy: containers and
		// bartering, the workbench and inspect and the power armor station,
		// and cooking.
		constexpr CallSite HELPER_SITES[] = {
			{ AE{ 2222624, 0x0E6 }, OG{ 223535, 0x0A8 }, "container card" },
			{ AE{ 2223053, 0x281 }, OG{ 579386, 0x240 }, "examine card" },
			{ AE{ 2222904, 0x0E9 }, OG{ 1053915, 0x0E9 }, "cooking card" },
		};

		// The calls to PipboyInventoryData::PopulateItemCardInfo, the same job
		// for the Pip-Boy. The first, in InitializeItem, is a tail call whose
		// return nothing reads, see PatchCall. It runs as the game lists an
		// item, which it does again after every wear of the item. The second
		// is a category rebuilt, on an equip and when ItemCards::Refresh asks.
		constexpr CallSite PIPBOY_SITES[] = {
			{ AE{ RE::ID::PipboyInventoryData::InitializeItem.ae(), 0x651, true }, OG{ RE::ID::PipboyInventoryData::InitializeItem.og(), 0x5A3, true }, "pipboy card" },
			{ AE{ RE::ID::PipboyInventoryData::RepopulateItemCardOnSection.ae(), 0x40F }, OG{ RE::ID::PipboyInventoryData::RepopulateItemCardOnSection.og(), 0x412 }, "pipboy card rebuild" },
		};

		// The 2 Pip-Boy sites, which go in together, see PopulatePipboy.
		CallPatch::Held g_pipboy;

		using Helper_t = void (*)(Scaleform::GFx::Value&, const RE::BGSInventoryItem&, std::uint32_t, const CompareItems&, bool);
		using Pipboy_t = void (*)(RE::PipboyInventoryData*, const RE::BGSInventoryItem*, const RE::BGSInventoryItem::Stack*, RE::PipboyObject*);

		std::array<CallPatch::Link<Helper_t>, std::size(HELPER_SITES)> g_helperLinks;
		std::array<CallPatch::Link<Pipboy_t>, std::size(PIPBOY_SITES)> g_pipboyLinks;

		// Moves the entry just appended to the front. The card turns entries
		// into rows from last to first, so the first entry becomes the highest
		// plain row, directly under Damage, where it stays in a menu the
		// render listener in Raise.cpp cannot reach.
		void MoveLastToFront(Value& a_entries)
		{
			const auto size = a_entries.GetArraySize();
			if (size < 2) {
				return;
			}

			Value last;
			a_entries.GetElement(size - 1, &last);
			for (auto i = size - 1; i > 0; i--) {
				Value previous;
				a_entries.GetElement(i - 1, &previous);
				a_entries.SetElement(i, previous);
			}
			a_entries.SetElement(0, last);
		}

		// The number on the CND row: a weapon's or armor's condition, or how
		// fresh a food is.
		std::optional<double> RowPercent(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack)
		{
			const auto gear = Condition::Percent(a_item, a_stack);
			return gear ? gear : FoodPercent(a_item, a_stack);
		}

		// Stands in for InventoryUserUIUtils::PopulateItemCardInfo_Helper. A
		// menu refreshing its card passes the card's array. A menu building
		// its list passes a list entry, which gets the array as its
		// ItemCardInfoList member. The last 2 arguments go through untouched.
		template <std::size_t I>
		void PopulateHelperHk(Value& a_target, const RE::BGSInventoryItem& a_item, std::uint32_t a_stackID,
			const CompareItems& a_compareItems, bool a_compareArmorWeightAndValue)
		{
			const auto*    stack = a_item.GetStackByID(a_stackID);
			const Building building{ Condition::HealthOf(stack), &a_compareItems };

			t_building = g_helperLinks[I].Live() ? &building : nullptr;
			g_helperLinks[I](a_target, a_item, a_stackID, a_compareItems, a_compareArmorWeightAndValue);
			const auto live = std::exchange(t_building, nullptr) != nullptr;

			const auto percent = live ? RowPercent(a_item, stack) : std::nullopt;
			if (!percent) {
				return;
			}

			Value entries;
			if (a_target.IsArray()) {
				entries = a_target;
			} else if (!a_target.GetMember("ItemCardInfoList"sv, &entries) || !entries.IsArray()) {
				return;
			}

			// The engine's own entry builder, so the row carries every field
			// the card reads. showAsPercent prints 91 as 91%.
			Value entry;
			RE::InventoryUserUIUtils::AddItemCardInfoEntry(entries, entry, CND_TEXT, Value(*percent));
			entry.SetMember("showAsPercent"sv, Value(true));
			if (InFridge(a_item, stack)) {
				entry.SetMember(FRIDGE_MARK, Value(true));
			}
			MoveLastToFront(entries);
		}

		// Stands in for PipboyInventoryData::PopulateItemCardInfo. The Pip-Boy
		// keeps its cards as a tree of values of its own, copied into Scaleform
		// objects when a page shows a card, so the row is built from those.
		// a_link is the calling site's.
		//
		// Both sites build the same card, so while either is left to another
		// mod NEC leaves the card as the game builds it: no CND row, the fire
		// rate, accuracy and range of an unworn gun and no faded name. With 1
		// site the row would come and go.
		void PopulatePipboy(RE::PipboyInventoryData* a_this, const RE::BGSInventoryItem* a_item,
			const RE::BGSInventoryItem::Stack* a_stack, RE::PipboyObject* a_data, const CallPatch::Link<Pipboy_t>& a_link)
		{
			const Building building{ Condition::HealthOf(a_stack), nullptr };

			t_building = g_pipboy.Runs(a_link) ? &building : nullptr;
			a_link(a_this, a_item, a_stack, a_data);
			const auto live = std::exchange(t_building, nullptr) != nullptr;

			const auto percent = live && a_item && a_data ? RowPercent(*a_item, a_stack) : std::nullopt;
			if (!percent) {
				return;
			}

			// The item's own entry, which the Pip-Boy numbers as it builds it.
			// Pipboy::MarkBroken keeps the number of a worn out item or of
			// spoiled food, which reads 0, to fade its name. It runs either
			// way, since a repaired item needs its number forgotten.
			const auto spent = Condition::WearsOut(*a_item->object) ? a_stack && a_stack->extra && a_stack->extra->IsItemBroken() : *percent == 0.0;
			Pipboy::MarkBroken(*a_data, a_stack, spent);

			// The call above created this array from scratch, so its order is
			// still free to change.
			auto* entries = a_data->GetMember<RE::PipboyArray*>(RE::BSFixedString("itemCardInfoList"));
			if (!entries) {
				return;
			}

			const RE::BSFixedStringCS text{ CND_TEXT };
			a_this->AddItemCardInfoEntry(&text, static_cast<float>(*percent), entries);

			// The helper appends the entry without handing it back.
			auto& elements = entries->elements;
			if (elements.empty()) {
				return;
			}
			auto* entry = static_cast<RE::PipboyObject*>(elements.back());

			const RE::BSFixedString showAsPercent{ "showAsPercent" };
			entry->AddMember(&showAsPercent, new RE::PipboyPrimitiveValue<bool>(true, entry));

			std::rotate(elements.begin(), elements.end() - 1, elements.end());
		}

		// PopulatePipboy for the first site, where the Pip-Boy lists an item.
		// The trace names each entry of an item that wears, which shows a
		// split stack reached the Pip-Boy as its own rows.
		void PipboyNewEntryHk(RE::PipboyInventoryData* a_this, const RE::BGSInventoryItem* a_item,
			const RE::BGSInventoryItem::Stack* a_stack, RE::PipboyObject* a_data)
		{
			PopulatePipboy(a_this, a_item, a_stack, a_data, g_pipboyLinks[0]);

			if (!TraceLog::IsOpen() || !a_item || !a_item->object || !a_stack) {
				return;
			}
			const auto percent = Condition::Percent(*a_item, a_stack);
			if (percent) {
				// The extra data list, to match against the one the spawn roll
				// wrote to, see SpawnCondition/Roll.cpp.
				auto* const extra = a_stack->extra.get();
				const auto* legendary = extra ? extra->GetLegendaryMod() : nullptr;
				TraceLog::Line("menu", "Pip-Boy lists {:s} [{:08X}] x{:d} at {:.0f}% from extra data {:p}, {:s}",
					RE::TESFullName::GetFullName(*a_item->object), a_item->object->formID, a_stack->GetCount(), *percent,
					static_cast<const void*>(extra), legendary ? std::format("legendary {}", TraceLog::Who{ legendary }) : "no legendary");
			}
		}

		// The second site, where a card is rebuilt.
		void PipboyPopulateHk(RE::PipboyInventoryData* a_this, const RE::BGSInventoryItem* a_item,
			const RE::BGSInventoryItem::Stack* a_stack, RE::PipboyObject* a_data)
		{
			PopulatePipboy(a_this, a_item, a_stack, a_data, g_pipboyLinks[1]);
		}
	}

	bool PipboyCards()
	{
		return g_pipboy.Intact();
	}

	bool PatchCards()
	{
		const auto helperHooks = CallPatch::PerSite<std::size(HELPER_SITES)>([]<std::size_t I>() { return &PopulateHelperHk<I>; });
		const auto helper = CallPatch::PatchAll(HELPER_SITES, RE::ID::InventoryUserUIUtils::PopulateItemCardInfo_Helper, helperHooks,
			g_helperLinks, "Menu item cards show a CND row");

		// The 2 Pip-Boy sites build the same card, so they go in together, see
		// PopulatePipboy.
		const auto& populate = RE::ID::PipboyInventoryData::PopulateItemCardInfo;
		g_pipboy = CallPatch::PatchTogether({
			{ PIPBOY_SITES[0], populate, reinterpret_cast<std::uintptr_t>(&PipboyNewEntryHk), &g_pipboyLinks[0] },
			{ PIPBOY_SITES[1], populate, reinterpret_cast<std::uintptr_t>(&PipboyPopulateHk), &g_pipboyLinks[1] },
		});
		if (g_pipboy) {
			REX::INFO("Pip-Boy item cards show a CND row.");
		}

		return helper > 0 || static_cast<bool>(g_pipboy);
	}
}
