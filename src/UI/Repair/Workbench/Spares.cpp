#include "UI/Repair/Workbench/Spares.h"

#include "Condition/Condition.h"
#include "Condition/CraftingPerks/CraftingPerks.h"
#include "Condition/Materials/Yield.h"
#include "Condition/Repair.h"
#include "Core/Settings/Settings.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Repair/Workbench/Lock.h"
#include "UI/Repair/Workbench/SpareMods.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>

namespace Workbench
{
	namespace
	{
		using Condition::FULL_TENTHS;
		using Condition::TENTHS_PER_PERCENT;
		using Repair::DebtTenths;
		using Repair::FULL;

		// 1 stack of the item's base in the player's inventory, read under
		// the inventory's lock. The list is held, so it outlives the lock.
		struct Copy
		{
			RE::BSTSmartPointer<RE::ExtraDataList> extra;
			std::uint32_t                          count{ 0 };
			// The whole percent the scrap yields count from,
			// and the number the card shows, see Condition::CardTenths.
			std::uint32_t                          percent{ FULL };
			std::uint32_t                          tenths{ Condition::FULL_TENTHS };
			bool                                   equipped{ false };
			bool                                   withItem{ false };
			std::string                            name;
		};

		// Every stack of a_item's base the player holds, or none when the
		// item itself is not among them, as for a copy the bench lists from
		// somewhere else.
		std::vector<Copy> Copies(const Selection& a_item)
		{
			std::vector<Copy> out;
			auto*             player = RE::PlayerCharacter::GetSingleton();
			auto*             list = player ? player->inventoryList : nullptr;
			if (!list || !a_item.object) {
				return out;
			}

			bool                     held = false;
			const RE::BSAutoReadLock l{ list->rwLock };
			for (const auto& entry : list->data) {
				if (entry.object != a_item.object) {
					continue;
				}
				for (const auto* stack = entry.stackData.get(); stack; stack = stack->nextStack.get()) {
					const auto* extra = stack->extra.get();
					const auto  percent = Condition::Percent(entry, stack);
					const auto  tenths = Condition::Tenths(entry, stack);
					const auto* shown = entry.GetDisplayFullName(extra);
					held = held || extra == a_item.extra;
					out.push_back({ stack->extra, stack->GetCount(),
						percent ? static_cast<std::uint32_t>(*percent) : FULL, tenths.value_or(Condition::FULL_TENTHS),
						stack->IsEquipped(), extra == a_item.extra,
						shown && *shown != '\0' ? shown : std::string{ RE::TESFullName::GetFullName(*entry.object) } });
				}
			}
			return held ? out : std::vector<Copy>{};
		}

		// iUnscrappableSpares at 0 and 2. Any other number reads as 1, see
		// Unscrappable.
		constexpr std::int32_t NO_UNSCRAPPABLE = 0;
		constexpr std::int32_t ALL_UNSCRAPPABLE = 2;

		// Whether iUnscrappableSpares keeps a spare out for the keyword the
		// SCRAP button refuses, the one the default object
		// UnscrappableObject_DO names. In vanilla the record carries it on the
		// unique weapons like Grognak's Axe and the Cryolator, on grenades,
		// mines and power armor pieces, and every legendary mod adds it to the
		// item itself. At 0 the spare's own keywords count where it has some,
		// as the SCRAP button reads them, so no legendary is listed. At 1 only
		// the record's count, so a legendary is, and at 2 the keyword counts
		// nowhere.
		[[nodiscard]] bool Unscrappable(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra)
		{
			const auto setting = Settings::iUnscrappableSpares.GetValue();
			if (setting == ALL_UNSCRAPPABLE) {
				return false;
			}
			auto*       store = RE::TESForm::GetFormByEditorID<RE::BGSDefaultObject>("UnscrappableObject_DO");
			const auto* keyword = store ? store->GetForm<RE::BGSKeyword>() : nullptr;
			const auto* data = a_extra && setting == NO_UNSCRAPPABLE ? a_extra->GetByType<RE::ExtraInstanceData>() : nullptr;
			const auto* instance = data ? data->data.get() : nullptr;
			if (!keyword) {
				return false;
			}
			if (const auto* weapon = a_object.As<RE::TESObjectWEAP>()) {
				return weapon->HasKeyword(keyword, instance);
			}
			const auto* armor = a_object.As<RE::TESObjectARMO>();
			return armor && armor->HasKeyword(keyword, instance);
		}

		// A quest item is never a spare, and gear the SCRAP button refuses only
		// as iUnscrappableSpares allows.
		[[nodiscard]] bool Refused(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra)
		{
			return (a_extra && a_extra->IsQuestObject()) || Unscrappable(a_object, a_extra);
		}

		// A favorite has a quick key slot, or the heart alone at -2 for none,
		// which reads as no favorite, as ExtraDataList::IsFavorite reads it.
		[[nodiscard]] bool Favorite(const RE::ExtraDataList* a_extra)
		{
			const auto* favorite = a_extra ? a_extra->GetByType<RE::ExtraFavorite>() : nullptr;
			return favorite && favorite->quickkeyIndex != -2;
		}

		// What the yield counts with nothing scaled, the components a spare
		// holds.
		constexpr std::uint32_t AS_HELD = 10;

		// fSpareWorthMult in whole tenths, the slider's step, kept to the
		// slider's 0.5 to 2.
		[[nodiscard]] std::uint32_t WorthTenths()
		{
			const auto tenths = std::lround(Settings::fSpareWorthMult.GetValue() * 10.0);
			return static_cast<std::uint32_t>(std::clamp(tenths, 5L, 20L));
		}

		[[nodiscard]] std::uint32_t WorthOf(const std::vector<Materials::Part>& a_parts)
		{
			std::uint32_t worth = 0;
			for (const auto& part : a_parts) {
				worth += part.count * static_cast<std::uint32_t>(std::max(part.component->value, 0));
			}
			return worth;
		}

		// The price of a repair from a_from, the card's number in tenths, up by
		// a_gain whole steps, in worth.
		[[nodiscard]] double PriceOf(const Priced& a_priced, std::uint32_t a_from, std::uint32_t a_gain)
		{
			const auto to = std::min(a_from + a_gain * TENTHS_PER_PERCENT, FULL_TENTHS);
			return BillWorth(a_priced) * (static_cast<double>(DebtTenths(a_from, a_priced.multiple)) - DebtTenths(to, a_priced.multiple));
		}

		// Covered, but LEAST_GAIN at least and never past full.
		[[nodiscard]] std::uint32_t GainOf(const Priced& a_priced, std::uint32_t a_from, std::uint32_t a_worth)
		{
			return std::min(std::max(Covered(a_priced, a_from, a_worth), LEAST_GAIN), Repair::PercentFrom(FULL_TENTHS - std::min(a_from, FULL_TENTHS)));
		}

		// a_copy as a spare for a_item, or nothing for a stack that never is
		// one, with the mods the plan a_kept keeps left whole.
		[[nodiscard]] std::optional<Spare> Worked(const Copy& a_copy, const Selection& a_item, const Priced& a_priced,
			std::span<const std::uint32_t> a_kept)
		{
			const auto* extra = a_copy.extra.get();
			if (Refused(*a_item.object, extra)) {
				return std::nullopt;
			}

			Spare spare;
			spare.extra = extra;
			spare.name = a_copy.name;
			spare.percent = a_copy.percent;
			spare.tenths = a_copy.tenths;
			spare.equipped = a_copy.equipped;
			spare.favorite = Favorite(extra);
			spare.legendary = extra && const_cast<RE::ExtraDataList*>(extra)->GetLegendaryMod();
			spare.withItem = a_copy.withItem;
			const auto kept = ReadMods(spare, extra, a_kept);
			const auto tenths = WorthTenths();
			spare.scrap = Materials::Yield(*a_item.object, extra, a_copy.percent, tenths, true, kept);
			spare.held = tenths > AS_HELD ? Materials::Yield(*a_item.object, extra, a_copy.percent, AS_HELD, true, kept) : spare.scrap;
			spare.worth = WorthOf(spare.scrap);
			spare.gain = GainOf(a_priced, a_item.tenths, spare.worth);
			const auto restores = [&a_item](const Priced& a_at, std::uint32_t a_worth) {
				return RestoredTenths(a_item, a_at, a_worth, GainOf(a_at, a_item.tenths, a_worth));
			};
			spare.restores = restores(a_priced, spare.worth);
			const auto ranked = CraftingPerks::Scrapper().rank > 0;
			spare.unranked = ranked ? restores(a_priced, WorthOf(Materials::Yield(*a_item.object, extra, a_copy.percent, tenths, false, kept))) :
			                          spare.restores;
			spare.unpriced = spare.restores;
			if (a_priced.standing.rank > 0) {
				auto unskilled = a_priced;
				unskilled.standing.rank = 0;
				unskilled.multiple = MultipleOf(unskilled.standing);
				spare.unpriced = restores(unskilled, spare.worth);
			}
			return spare;
		}

		// The items on a_copy's stack that are spares: all but the item
		// itself.
		[[nodiscard]] std::uint32_t SparesIn(const Copy& a_copy)
		{
			return a_copy.withItem ? a_copy.count - std::min(a_copy.count, 1U) : a_copy.count;
		}
	}

	double BillWorth(const Priced& a_priced)
	{
		double worth = 0.0;
		for (const auto& part : a_priced.built) {
			worth += static_cast<double>(part.count) * std::max(part.component->value, 0);
		}
		return worth;
	}

	std::uint32_t Covered(const Priced& a_priced, std::uint32_t a_from, std::uint32_t a_worth)
	{
		const auto    missing = Repair::PercentFrom(FULL_TENTHS - std::min(a_from, FULL_TENTHS));
		std::uint32_t covered = 0;
		while (covered < missing && PriceOf(a_priced, a_from, covered + 1) <= a_worth) {
			covered++;
		}
		return covered;
	}

	std::optional<Selection> Held(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* list = player ? player->inventoryList : nullptr;
		if (!list) {
			return std::nullopt;
		}

		const RE::BSAutoReadLock l{ list->rwLock };
		for (const auto& entry : list->data) {
			if (entry.object != &a_object) {
				continue;
			}
			std::uint32_t id = 0;
			for (const auto* stack = entry.stackData.get(); stack; stack = stack->nextStack.get(), id++) {
				if (stack->extra.get() != a_extra) {
					continue;
				}
				// Read as SelectedItem::Read reads the row, so the box and the
				// bench count the item alike.
				const auto tenths = Condition::Tenths(entry, stack);
				Selection  out;
				out.item = &entry;
				out.object = entry.object;
				out.extra = a_extra;
				out.stack = id;
				out.count = stack->GetCount();
				out.SetTenths(tenths.value_or(Condition::FULL_TENTHS));
				out.kind = Condition::KindOf(*entry.object);
				return out;
			}
		}
		return std::nullopt;
	}

	bool CarriesSpare(const Selection& a_item)
	{
		return SparesCarried(a_item) > 0;
	}

	std::uint32_t SparesCarried(const Selection& a_item)
	{
		if (!Settings::bSpareRepair.GetValue() || !a_item.Worn()) {
			return 0;
		}
		std::size_t count = 0;
		for (const auto& copy : Copies(a_item)) {
			if (const auto copies = SparesIn(copy); copies > 0 && !Refused(*a_item.object, copy.extra.get())) {
				count += copies;
			}
		}
		return static_cast<std::uint32_t>(std::min(count, MOST_SPARES));
	}

	bool SpareCanExist(const Selection& a_item)
	{
		return Settings::bSpareRepair.GetValue() && a_item.object && !Unscrappable(*a_item.object, nullptr);
	}

	std::vector<Spare> Spares(const Selection& a_item, const Priced& a_priced)
	{
		std::vector<Spare> out;
		if (!a_item.Worn()) {
			return out;
		}

		for (const auto& copy : Copies(a_item)) {
			const auto copies = SparesIn(copy);
			const auto spare = copies > 0 ? Worked(copy, a_item, a_priced, {}) : std::nullopt;
			if (!spare) {
				continue;
			}
			TraceLog::Line("menu", "Workbench lists {:d} of {:s} at {:s}% as {:s} for {:s} at {:s}%: it scraps into {:s}, worth {:d}, for +{:s}%",
				copies, spare->name, Condition::CardText(spare->tenths), copies == 1 ? "a spare" : "spares", a_item.Name(), Condition::CardText(a_item.tenths),
				Spell(spare->scrap), spare->worth, Condition::CardText(spare->restores));
			out.insert(out.end(), std::min<std::size_t>(copies, MOST_SPARES), *spare);
		}

		// The spares players most likely want gone come first, and a list too
		// long for the box drops the ones that restore the most.
		std::ranges::stable_sort(out, [](const Spare& a_left, const Spare& a_right) {
			if (a_left.restores != a_right.restores) {
				return a_left.restores < a_right.restores;
			}
			return a_left.tenths != a_right.tenths ? a_left.tenths < a_right.tenths : a_left.name < a_right.name;
		});
		if (out.size() > MOST_SPARES) {
			out.erase(out.begin() + MOST_SPARES, out.end());
		}
		return out;
	}

	std::optional<Spare> SpareOn(const Selection& a_item, const Priced& a_priced, const RE::ExtraDataList* a_extra,
		std::span<const std::uint32_t> a_kept)
	{
		for (const auto& copy : Copies(a_item)) {
			if (copy.extra.get() == a_extra && SparesIn(copy) > 0) {
				return Worked(copy, a_item, a_priced, a_kept);
			}
		}
		return std::nullopt;
	}

	std::optional<std::uint32_t> SpareGain(const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_item,
		const RE::ExtraDataList* a_spare)
	{
		if (!Repairs() || !Settings::bSpareRepair.GetValue()) {
			return std::nullopt;
		}
		auto item = Held(a_object, a_item);
		if (!item || !item->Worn()) {
			return std::nullopt;
		}
		// The entry Held points at is the player's, which may change once its
		// lock is let go, so the name comes from the form.
		item->item = nullptr;
		const auto priced = PriceAsLastRead(*item);
		// The press opens no box of spares then, see Workbench.cpp. HeldBack
		// reads nothing of the entry let go above.
		if (Settings::bLockSpares.GetValue() && HeldBack(*item, priced)) {
			return std::nullopt;
		}
		const auto spare = priced.units > 0 ? SpareOn(*item, priced, a_spare) : std::nullopt;
		return spare ? std::optional{ spare->restores / TENTHS_PER_PERCENT } : std::nullopt;
	}

	Payment Pay(const Spare& a_spare, const Selection& a_item, const Priced& a_priced)
	{
		auto cheapest = a_spare.scrap;
		std::ranges::sort(cheapest, [](const Materials::Part& a_left, const Materials::Part& a_right) {
			const auto left = a_left.component->value;
			const auto right = a_right.component->value;
			return left != right ? left < right : a_left.component->formID < a_right.component->formID;
		});

		Payment out;
		auto    owed = PriceOf(a_priced, a_item.tenths, a_spare.gain);
		for (const auto& part : cheapest) {
			const auto    worth = static_cast<double>(std::max(part.component->value, 0));
			std::uint32_t took = 0;
			while (took < part.count && owed > 0.0 && owed * 2.0 >= worth) {
				took++;
				owed -= worth;
			}
			if (took > 0) {
				out.taken.push_back({ part.component, took });
			}
			if (took < part.count) {
				break;
			}
		}
		if (out.taken.empty() && !cheapest.empty()) {
			out.taken.push_back({ cheapest.front().component, 1 });
		}

		// What goes back is worked from the components the spare holds, so a
		// worth above 1 never hands back more than the spare is made of.
		for (const auto& part : a_spare.held) {
			const auto taken = std::ranges::find(out.taken, part.component, &Materials::Part::component);
			const auto used = taken != out.taken.end() ? taken->count : 0U;
			if (part.count > used) {
				out.back.push_back({ part.component, part.count - used });
			}
		}
		return out;
	}

	std::vector<std::string> CellsOf(const Spare& a_spare)
	{
		return { Text::SpareMark(a_spare.equipped, a_spare.favorite, a_spare.legendary), a_spare.name,
			Text::SpareCondition(Condition::CardText(a_spare.tenths)), Text::SpareRestores(Condition::CardText(a_spare.restores)) };
	}
}
