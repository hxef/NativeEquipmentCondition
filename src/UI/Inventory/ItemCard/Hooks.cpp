#include "UI/Inventory/ItemCard/Cards.h"

#include "Condition/Condition.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Inventory/Pipboy.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <string_view>

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

		// The examine card's place in HELPER_SITES, whose order sets it: the
		// workbench and inspect, see ExamineMenu::UpdateItemCard.
		constexpr std::size_t EXAMINE_SITE = 1;
		static_assert(std::string_view{ HELPER_SITES[EXAMINE_SITE].what } == "examine card"sv);

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

		// Moves the entry just appended to a_index, and the entries from there
		// on 1 place back.
		void MoveLastTo(Value& a_entries, std::uint32_t a_index)
		{
			if (const auto size = a_entries.GetArraySize(); size > 0) {
				MoveEntry(a_entries, size - 1, a_index);
			}
		}

		// Moves the entry just appended to the front. The card turns entries
		// into rows from last to first, so the first entry becomes the highest
		// plain row, directly under Damage, where it stays in a menu the
		// render listener in Raise.cpp cannot reach.
		void MoveLastToFront(Value& a_entries)
		{
			MoveLastTo(a_entries, 0);
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
			const auto     live = g_helperLinks[I].Live();
			auto           food = live ? FoodCardOf(a_item, stack) : FoodCard{};

			t_building = live ? &building : nullptr;
			t_foodCard = live ? &food : nullptr;
			g_helperLinks[I](a_target, a_item, a_stackID, a_compareItems, a_compareArmorWeightAndValue);
			t_building = nullptr;
			t_foodCard = nullptr;
			if (live) {
				ShowPowerArmorCnd(a_target, a_item, stack, HELPER_SITES[I].what);
			}

			const auto percent = live ? RowPercent(a_item, stack) : std::nullopt;
			const auto rads = RadsRow(food);
			if (!percent && !rads) {
				return;
			}

			Value entries;
			if (!CardEntries(a_target, entries)) {
				return;
			}

			if (rads) {
				Value row;
				RE::InventoryUserUIUtils::AddItemCardInfoEntry(entries, row, RadsText(), Value(static_cast<double>(*rads)));
				const auto size = entries.GetArraySize();
				MoveLastTo(entries, size > CLOSING_ROWS ? size - 1 - CLOSING_ROWS : size - 1);
			}
			if (!percent) {
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
			if (const auto rate = WearRateOf(a_item, stack)) {
				entry.SetMember(WEAR_RATE_MARK, Value(static_cast<double>(*rate)));

				// At a workbench a_item is the bench's own copy, with the mod
				// the player highlights. The 1 item it is compared with is the
				// item itself while a mod is tried on, else the copy again. So
				// an entry that is not the copy but has its base form gives the
				// rate without the mod, which signs the change, see
				// WriteWearRate.
				if constexpr (I == EXAMINE_SITE) {
					const auto* compared = a_compareItems.size() == 1 ? a_compareItems[0].first : nullptr;
					if (compared && compared != &a_item && compared->object == a_item.object) {
						if (const auto own = WearRateOf(*compared, compared->GetStackByID(a_compareItems[0].second))) {
							entry.SetMember(WEAR_RATE_COMPARED_MARK, Value(static_cast<double>(*own)));
						}
					}
				}
			}
			MoveLastToFront(entries);
		}

		// Adds the Rads row a card of spoiled food needs to a_data, the
		// Pip-Boy's card, just before its closing rows.
		void AddRadsRow(RE::PipboyInventoryData& a_this, RE::PipboyObject& a_data, float a_rads)
		{
			auto* entries = a_data.GetMember<RE::PipboyArray*>(RE::BSFixedString("itemCardInfoList"));
			if (!entries) {
				return;
			}
			const RE::BSFixedStringCS text{ RadsText() };
			a_this.AddItemCardInfoEntry(&text, a_rads, entries);
			auto& elements = entries->elements;
			if (elements.size() > CLOSING_ROWS + 1) {
				std::rotate(elements.end() - 1 - CLOSING_ROWS, elements.end() - 1, elements.end());
			}
		}

		// Stands in for PipboyInventoryData::PopulateItemCardInfo. The Pip-Boy
		// keeps its cards as a tree of values of its own, copied into Scaleform
		// objects when a page shows a card, so the row is built from those.
		// a_site is the calling site's place in PIPBOY_SITES.
		//
		// Both sites build the same card, so while either is left to another
		// mod NEC leaves the card as the game builds it: no CND row, the fire
		// rate, accuracy and range of an unworn gun and no faded name. With 1
		// site the row would come and go.
		void PopulatePipboy(RE::PipboyInventoryData* a_this, const RE::BGSInventoryItem* a_item,
			const RE::BGSInventoryItem::Stack* a_stack, RE::PipboyObject* a_data, std::size_t a_site)
		{
			const auto&    link = g_pipboyLinks[a_site];
			const Building building{ Condition::HealthOf(a_stack), nullptr };
			const auto     live = g_pipboy.Runs(link);
			auto           food = live && a_item ? FoodCardOf(*a_item, a_stack) : FoodCard{};

			t_building = live ? &building : nullptr;
			t_foodCard = live ? &food : nullptr;
			link(a_this, a_item, a_stack, a_data);
			t_building = nullptr;
			t_foodCard = nullptr;
			if (live && a_item && a_data) {
				ShowPowerArmorCnd(*a_data, *a_item, a_stack, PIPBOY_SITES[a_site].what);
			}

			if (const auto rads = a_data ? RadsRow(food) : std::nullopt) {
				AddRadsRow(*a_this, *a_data, *rads);
			}
			const auto percent = live && a_item && a_data ? RowPercent(*a_item, a_stack) : std::nullopt;
			if (!percent) {
				return;
			}

			// The item's own entry, which the Pip-Boy numbers as it builds it.
			// Pipboy::MarkEntry keeps the number of a worn out item or of
			// spoiled food, which reads 0, to fade its name, and the wear rate
			// its card shows, since no member of NEC's own reaches the card. It
			// runs either way, since a repaired item needs its number
			// forgotten.
			const auto spent = Condition::WearsOut(*a_item->object) ? a_stack && a_stack->extra && a_stack->extra->IsItemBroken() : *percent == 0.0;
			Pipboy::MarkEntry(*a_data, a_stack, spent, WearRateOf(*a_item, a_stack));

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
			PopulatePipboy(a_this, a_item, a_stack, a_data, 0);

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
			PopulatePipboy(a_this, a_item, a_stack, a_data, 1);
		}
	}

	bool CardEntries(Value& a_target, Value& a_out)
	{
		if (a_target.IsArray()) {
			a_out = a_target;
			return true;
		}
		return a_target.GetMember("ItemCardInfoList"sv, &a_out) && a_out.IsArray();
	}

	void MoveEntry(Value& a_entries, std::uint32_t a_from, std::uint32_t a_to)
	{
		Value moved;
		if (a_to >= a_from || a_from >= a_entries.GetArraySize() || !a_entries.GetElement(a_from, &moved)) {
			return;
		}
		for (auto i = a_from; i > a_to; i--) {
			Value previous;
			a_entries.GetElement(i - 1, &previous);
			a_entries.SetElement(i, previous);
		}
		a_entries.SetElement(a_to, moved);
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
