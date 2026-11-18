#include "UI/Repair/Workbench/SpareBox.h"

#include "Condition/Condition.h"
#include "Condition/CraftingPerks/CraftingPerks.h"
#include "Condition/Equipped.h"
#include "Core/Settings/Settings.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Repair/Columns/Columns.h"
#include "UI/Repair/Gains.h"
#include "UI/Repair/RepairPrompt.h"
#include "UI/Repair/Restore.h"
#include "UI/Repair/Workbench/Box.h"
#include "UI/Repair/Workbench/Job.h"
#include "UI/Repair/Workbench/Lock.h"
#include "UI/Repair/Workbench/SpareModBox.h"
#include "UI/Repair/Workbench/SpareMods.h"
#include "UI/Repair/Workbench/Spares.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Workbench
{
	namespace
	{
		using Condition::FULL_TENTHS;

		// -------------------------------------------------------------------
		// Finding the stacks again
		// -------------------------------------------------------------------

		// Takes 1 copy off the player's stack of a_object with a_extra, its
		// number read right before, through the player's own RemoveItem,
		// which takes an equipped copy off first. False once that stack is
		// gone.
		bool RemoveOne(RE::PlayerCharacter& a_player, RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra)
		{
			const auto held = Held(a_object, a_extra);
			if (!held) {
				return false;
			}
			RE::TESObjectREFR::RemoveItemData take{ &a_object, 1 };
			take.stackData.push_back(held->stack);
			a_player.RemoveItem(take);
			return true;
		}

		// -------------------------------------------------------------------
		// What comes back
		// -------------------------------------------------------------------

		// The components to the player, each as its scrap item, the way the
		// SCRAP button hands them out. The player is told in the corner, not by
		// vanilla's own lines, see SpareBox.h.
		void GiveBack(RE::PlayerCharacter& a_player, const std::vector<Materials::Part>& a_parts)
		{
			const RE::PlayerCharacter::ScopedInventoryChangeMessageContext quiet{ true, true };
			for (const auto& part : a_parts) {
				if (auto* item = part.component->scrapItem) {
					a_player.AddInventoryItem(item, nullptr, part.count, nullptr, nullptr, nullptr);
				}
			}
		}

		// "Salvaged Steel, Screw (2) and Spring.", or nothing for no
		// components.
		std::string Salvaged(const std::vector<Materials::Part>& a_parts)
		{
			std::vector<std::string> names;
			for (const auto& part : a_parts) {
				const RE::TESForm* item = part.component->scrapItem;
				const auto         name = RE::TESFullName::GetFullName(item ? *item : *part.component);
				names.push_back(part.count > 1 ? std::format("{:s} ({:d})", name, part.count) : std::string{ name });
			}
			return names.empty() ? std::string{} : Text::SpareSalvaged(names);
		}

		// The crafting loop over the components taken, or over the spare itself
		// when it gave none, as MEND plays it over the item.
		void PlaySound(RE::ExamineMenu& a_menu, RE::TESBoundObject& a_object, const std::vector<Materials::Part>& a_taken)
		{
			for (const auto& part : a_taken) {
				a_menu.QueueCraftingComponent(const_cast<RE::BGSComponent*>(part.component));
			}
			if (a_taken.empty()) {
				a_menu.QueueCraftingComponent(&a_object);
			}
			PlayRepairSound(&a_menu);
		}
	}

	// -----------------------------------------------------------------------
	// The pick
	// -----------------------------------------------------------------------

	void Pick(RE::TESBoundObject& a_object, const RE::ExtraDataList* a_item, const RE::ExtraDataList* a_spare,
		const Priced& a_priced, const std::optional<std::vector<std::uint32_t>>& a_plan)
	{
		auto* menu = OpenBench();
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!menu || !player || !RepairsIn(menu)) {
			TraceLog::Line("menu", "Workbench dropped the spare, the bench is gone or repairs are off");
			return;
		}

		// The spare is worked out for its plan here, before anything
		// moves. Off keeps nothing, so it stays the whole spare.
		const auto item = Held(a_object, a_item);
		auto       spare = item && item->Worn() ? SpareOn(*item, a_priced, a_spare) : std::nullopt;
		if (spare) {
			TraceSplit(*spare);
			const auto plan = a_plan ? *a_plan : AutoKept(*item, a_priced, *spare, KeepModsNow());
			if (!plan.empty()) {
				spare = SpareOn(*item, a_priced, a_spare, plan);
			}
		}
		if (!spare) {
			TraceLog::Line("menu", "Workbench found the item or the spare gone, so it asks again with nothing used");
			AskWhichSpare(a_object, a_item, a_priced);
			return;
		}

		// Read before anything moves, since a write can merge the item's
		// stack away and an add can move the whole inventory.
		const auto name = item->Name();
		const auto from = item->tenths;
		const auto payment = Pay(*spare, *item, a_priced);
		bool       mended = false;
		const auto level = LandingOf(*spare, *item, a_priced, &mended);

		{
			const RE::PlayerCharacter::ScopedInventoryChangeMessageContext quiet{ true, true };
			if (!spare->withItem && !RemoveOne(*player, a_object, a_spare)) {
				TraceLog::Line("menu", "Workbench could not take the spare {:s}, so it repaired nothing", spare->name);
				return;
			}
		}

		Equipped::ListStack find{ a_item };
		Restore::WriteTenths(*player, a_object, find, level, ScriptEvents::Cause::kRepair);
		const auto* written = Written(a_object, a_item, item->count > 1, level, name);
		if (spare->withItem) {
			const RE::PlayerCharacter::ScopedInventoryChangeMessageContext quiet{ true, true };
			RemoveOne(*player, a_object, a_item);
		}

		GiveBack(*player, payment.back);
		Materials::Give(*player, spare->kept);
		// The bench counts from a copy it made as it opened, made again here
		// so a repair right after counts the components given back.
		menu->UpdateOptimizedAutoBuildInv();
		SetParts(payment.taken);
		const auto paid = PayExperience(*player);
		PlaySound(*menu, a_object, payment.taken);

		TraceLog::Line("menu",
			"Workbench used up {:s}{:s} at {:s}% on {:s}, from {:s}% to {:s}%{:s}, taking {:s} and giving back {:s}, worth {:d} for {:g} experience before Intelligence and perks",
			spare->equipped ? "the equipped " : "", spare->name, Condition::CardText(spare->tenths), name, Condition::CardText(from), Condition::CardText(level),
			mended ? std::format(", mended for nothing from {:s}% up", Condition::PointText(FreeFromTenths())) : std::string{},
			Spell(payment.taken), Spell(payment.back), paid.worth, paid.experience);
		TraceHandedBack(*spare);

		RefreshKept(*menu, a_object, written);

		if (const auto kept = KeptLine(spare->kept); !kept.empty()) {
			RE::SendHUDMessage::ShowHUDMessage(kept.c_str(), nullptr, true, true);
		}
		if (const auto said = Salvaged(payment.back); !said.empty()) {
			RE::SendHUDMessage::ShowHUDMessage(said.c_str(), nullptr, true, true);
		}
		if (level >= FULL_TENTHS || !written) {
			TraceLog::Line("menu", "Workbench asks no more, {:s} is at {:s}%", name, Condition::CardText(level));
			return;
		}
		AskWhichSpare(a_object, written, a_priced);
	}

	void AskWhichSpare(RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra, const Priced& a_priced)
	{
		const auto item = RepairsIn(OpenBench()) ? Held(a_object, a_extra) : std::nullopt;
		const auto spares = item ? Spares(*item, a_priced) : std::vector<Spare>{};
		if (spares.empty()) {
			TraceLog::Line("menu", "Workbench asks nothing, the item is gone, at full or has no spare left");
			return;
		}

		// Each row ends with what the item's card gains at the condition the
		// spare brings it to, the condition its percent column names, in
		// tenths.
		std::vector<std::uint32_t>            levels;
		std::vector<const RE::ExtraDataList*> lists;
		std::vector<bool>                     withMods;
		for (const auto& spare : spares) {
			levels.push_back(std::min(item->tenths + spare.restores, FULL_TENTHS));
			lists.push_back(spare.extra);
			withMods.push_back(!spare.mods.empty());
		}
		const auto     gains = Gains::ReadTenths(*item, levels);
		Columns::Table table;
		table.name = NAME_CELL;
		for (std::size_t i = 0; i < spares.size(); i++) {
			Gains::AddRow(table, CellsOf(spares[i]), gains, i);
			TraceLog::Line("menu", "Workbench spare {:d} brings {:s} to {:s}%, its card {:s}", i + 1, item->Name(), Condition::CardText(levels[i]),
				Gains::Spelled(gains, i));
		}
		auto* bench = OpenBench();
		auto  rows = Columns::Lay(bench ? bench->uiMovie.get() : nullptr, table, "Workbench spare box");

		// The line above the question names each rank that raises what a
		// listed spare restores, the perk pricing the item first, then
		// Scrapper, as the repair box names the perk pricing the item. A rank
		// that raises none is left off, so the line never claims a gain that
		// is not on the list.
		const auto& pricing = a_priced.standing;
		const auto  scrapper = CraftingPerks::Scrapper();
		const auto  byPrice = std::ranges::count_if(spares, [](const Spare& a_spare) { return a_spare.unpriced < a_spare.restores; });
		const auto  byScrapper = std::ranges::count_if(spares, [](const Spare& a_spare) { return a_spare.unranked < a_spare.restores; });
		std::string perks;
		if (byPrice > 0 && byScrapper > 0) {
			perks = Text::SparePerks(pricing.Name(), pricing.rank, scrapper.Name(), scrapper.rank);
		} else if (byPrice > 0) {
			perks = Text::SparePerk(pricing.Name(), pricing.rank);
		} else if (byScrapper > 0) {
			perks = Text::SparePerk(scrapper.Name(), scrapper.rank);
		}

		// While the repair lock holds the item back, its line comes first,
		// naming the rank the bench asks for. It is asked again each time the
		// box opens, so it is gone once a spare lifts the item clear. The line
		// names no trader, since the spares below restore the item right here.
		// It is left out where no repair takes components, in a Pip-Boy
		// inspect at iPipboyRepair 1, Spares only, since a spare repairs the
		// item there either way. Repairs at a bench and at iPipboyRepair 2
		// take components, so the line stays there. While bLockSpares is on
		// the lock opens no box for such an item, see LockStops. NAME_TRADER
		// true adds the trader's sentence while traders repair.
		constexpr bool NAME_TRADER = false;
		std::string over;
		const bool componentRepairs = !PipboyInspect(bench) || PipboyRepair() == 2;
		if (const auto held = componentRepairs ? HeldBack(*item, a_priced) : std::nullopt) {
			over = Text::RepairLocked(pricing.Name(), held->need, Condition::CardText(item->tenths), false, NAME_TRADER && Settings::bVendorRepair.GetValue());
		}
		if (!over.empty() && !perks.empty()) {
			over += '\n';
		}
		over += perks;
		std::string credit{ "nobody" };
		if (scrapper.rank > 0) {
			credit = byScrapper > 0 ? std::format("{:s} {:d}, which raises {:d} of them", scrapper.Name(), scrapper.rank, byScrapper) :
			                          std::format("nobody, as {:s} {:d} raises none of them", scrapper.Name(), scrapper.rank);
		}
		TraceLog::Line("menu", "Workbench asks which of {:d} spares to use up on {:s} at {:s}%, crediting {:s}",
			spares.size(), item->Name(), Condition::CardText(item->tenths), credit);
		if (pricing.perk && pricing.rank > 0) {
			TraceLog::Line("menu", "Workbench credits {:s} {:d}, the perk pricing {:s}, which raises {:d} of them", pricing.Name(),
				pricing.rank, item->Name(), byPrice);
		}

		// The bench, the item and the spare are looked up again when the
		// answer comes, in case anything moved in between. At Choose mods to
		// use up or to keep a spare with a mod that comes off asks which mods
		// to use up or keep first.
		RepairPrompt::AskRows(over, Text::SpareQuestion(item->Name(), Condition::CardText(item->tenths)), std::move(rows),
			[object = &a_object, a_extra, lists = std::move(lists), withMods = std::move(withMods), priced = a_priced](std::size_t a_row) {
				const auto setting = KeepModsNow();
				if (Settings::KeepSpareModsAsks(std::to_underlying(setting)) && withMods[a_row]) {
					AskMods(*object, a_extra, lists[a_row], priced, {}, setting);
					return;
				}
				Pick(*object, a_extra, lists[a_row], priced, std::nullopt);
			},
			[] { TraceLog::Line("menu", "Workbench spare box closed with nothing used"); });
	}

	// -------------------------------------------------------------------
	// Keeping the item on show
	// -------------------------------------------------------------------

	const RE::ExtraDataList* Written(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_was, bool a_split,
		std::uint32_t a_tenths, std::string_view a_name)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* list = player ? player->inventoryList : nullptr;
		if (!list) {
			return nullptr;
		}

		const RE::BSAutoReadLock l{ list->rwLock };
		for (const auto& entry : list->data) {
			if (entry.object != &a_object) {
				continue;
			}
			const auto at = [&entry](const RE::BGSInventoryItem::Stack* a_stack) {
				return Condition::Tenths(entry, a_stack).value_or(FULL_TENTHS);
			};
			const RE::ExtraDataList* before = nullptr;
			for (const auto* stack = entry.stackData.get(); stack; stack = stack->nextStack.get()) {
				const auto* extra = stack->extra.get();
				if ((!a_split && extra == a_was) || (a_split && before == a_was && at(stack) == a_tenths)) {
					return extra;
				}
				before = extra;
			}
			for (const auto* stack = entry.stackData.get(); stack; stack = stack->nextStack.get()) {
				const auto* shown = entry.GetDisplayFullName(stack->extra.get());
				if (at(stack) == a_tenths && shown && a_name == shown) {
					return stack->extra.get();
				}
			}
		}
		return nullptr;
	}

	namespace
	{
		// Keeps the bench's highlight on the item. The list's rebuild puts it
		// on the row whose item handle and stack number match the menu's own
		// record, which the bench writes only on a rename or a mod build. So
		// the record gets the item's handle from the item's row of the list
		// before the rebuild, swapped with the record's own, and the item's
		// stack number. Each handle keeps the 1 hold it had, and the rebuild
		// lets go of that row's as it lets go of every row's. Returns true
		// when it wrote the record.
		bool KeepHighlight(RE::ExamineMenu& a_menu, const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra)
		{
			auto*      inventory = RE::BGSInventoryInterface::GetSingleton();
			const auto held = a_extra ? Held(a_object, a_extra) : std::nullopt;
			if (!inventory || !held) {
				TraceLog::Line("menu", "Workbench left the highlight to the list, it found no stack of the item repaired");
				return false;
			}
			for (auto& row : a_menu.invInterface.stackedEntries) {
				const auto* item = inventory->RequestInventoryItem(row.invHandle.id);
				if (item && item->object == &a_object) {
					std::swap(a_menu.modItem.id, row.invHandle.id);
					a_menu.modStack = held->stack;
					return true;
				}
			}
			TraceLog::Line("menu", "Workbench left the highlight to the list, no row of it holds the item repaired");
			return false;
		}
	}

	void RefreshKept(RE::ExamineMenu& a_menu, const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_written)
	{
		const bool kept = KeepHighlight(a_menu, a_object, a_written);
		if (!kept && PipboyInspect(&a_menu)) {
			// The record still names the item the Pip-Boy first asked to
			// inspect, which a rebuild would go back to. On no stack the
			// list keeps the row it is on by its number.
			a_menu.modStack = std::numeric_limits<std::uint32_t>::max();
		}
		Refresh(&a_menu, a_object.GetFormType());
		if (kept) {
			// The record is left on no stack so later rebuilds keep the row
			// the player picks. The handle keeps its hold until the bench closes.
			a_menu.modStack = std::numeric_limits<std::uint32_t>::max();
		}
	}
}
