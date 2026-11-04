#include "UI/Repair/Workbench/Spares.h"

#include "Condition/Condition.h"
#include "Condition/Materials/Yield.h"
#include "Condition/Repair.h"
#include "Core/Settings.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>

namespace Workbench
{
	namespace
	{
		using Repair::Debt;
		using Repair::FULL;

		// 1 stack of the item's base in the player's inventory, read under
		// the inventory's lock. The list is held, so it outlives the lock.
		struct Copy
		{
			RE::BSTSmartPointer<RE::ExtraDataList> extra;
			std::uint32_t                          count{ 0 };
			std::uint32_t                          percent{ FULL };
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
					const auto* shown = entry.GetDisplayFullName(extra);
					held = held || extra == a_item.extra;
					out.push_back({ stack->extra, stack->GetCount(),
						percent ? static_cast<std::uint32_t>(*percent) : FULL, stack->IsEquipped(), extra == a_item.extra,
						shown && *shown != '\0' ? shown : std::string{ RE::TESFullName::GetFullName(*entry.object) } });
				}
			}
			return held ? out : std::vector<Copy>{};
		}

		// iUnscrappableSpares at 0 and 2. Any other number reads as 1, see
		// Unscrappable.
		constexpr std::int32_t NO_UNSCRAPPABLE = 0;
		constexpr std::int32_t ALL_UNSCRAPPABLE = 2;

		// Whether iUnscrappableSpares keeps the copy out for the keyword the
		// SCRAP button refuses, the one the default object
		// UnscrappableObject_DO names. In vanilla the record carries it on the
		// unique weapons like Grognak's Axe and the Cryolator, on grenades,
		// mines and power armor pieces, and every legendary mod adds it to the
		// copy. At 0 the copy's own keywords count where it has some, as the
		// SCRAP button reads them, so no legendary is listed. At 1 only the
		// record's count, so a legendary is, and at 2 the keyword counts
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

		// What the yield counts with nothing scaled, the parts a spare holds.
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

		// What the item's whole bill is worth, which a repair from 0 costs
		// a_priced.multiple times.
		[[nodiscard]] double BillWorth(const Priced& a_priced)
		{
			double worth = 0.0;
			for (const auto& part : a_priced.built) {
				worth += static_cast<double>(part.count) * std::max(part.component->value, 0);
			}
			return worth;
		}

		// The price of a repair from a_from up by a_gain, in worth.
		[[nodiscard]] double PriceOf(const Priced& a_priced, std::uint32_t a_from, std::uint32_t a_gain)
		{
			const auto to = std::min(a_from + a_gain, FULL);
			return BillWorth(a_priced) * (static_cast<double>(Debt(a_from, a_priced.multiple)) - Debt(to, a_priced.multiple));
		}

		// The largest whole step a_worth pays for from a_from, at least
		// LEAST_GAIN and never past full.
		[[nodiscard]] std::uint32_t GainOf(const Priced& a_priced, std::uint32_t a_from, std::uint32_t a_worth)
		{
			const auto    missing = FULL - std::min(a_from, FULL);
			std::uint32_t covered = 0;
			while (covered < missing && PriceOf(a_priced, a_from, covered + 1) <= a_worth) {
				covered++;
			}
			return std::min(std::max(covered, LEAST_GAIN), missing);
		}

		// a_copy as a spare for a_item, or nothing for a copy that never is
		// one.
		[[nodiscard]] std::optional<Spare> Worked(const Copy& a_copy, const Selection& a_item, const Priced& a_priced)
		{
			const auto* extra = a_copy.extra.get();
			if (Refused(*a_item.object, extra)) {
				return std::nullopt;
			}

			Spare spare;
			spare.extra = extra;
			spare.name = a_copy.name;
			spare.percent = a_copy.percent;
			spare.equipped = a_copy.equipped;
			spare.favorite = Favorite(extra);
			spare.legendary = extra && const_cast<RE::ExtraDataList*>(extra)->GetLegendaryMod();
			spare.withItem = a_copy.withItem;
			const auto tenths = WorthTenths();
			spare.scrap = Materials::Yield(*a_item.object, extra, a_copy.percent, tenths);
			spare.held = tenths > AS_HELD ? Materials::Yield(*a_item.object, extra, a_copy.percent, AS_HELD) : spare.scrap;
			spare.worth = WorthOf(spare.scrap);
			spare.gain = GainOf(a_priced, a_item.percent, spare.worth);
			return spare;
		}

		// The copies on a_copy's stack that are spares: all but the item
		// itself.
		[[nodiscard]] std::uint32_t SparesIn(const Copy& a_copy)
		{
			return a_copy.withItem ? a_copy.count - std::min(a_copy.count, 1U) : a_copy.count;
		}
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
				const auto percent = Condition::Percent(entry, stack);
				Selection  out;
				out.item = &entry;
				out.object = entry.object;
				out.extra = a_extra;
				out.stack = id;
				out.count = stack->GetCount();
				out.percent = percent ? static_cast<std::uint32_t>(*percent) : FULL;
				out.kind = Condition::KindOf(*entry.object);
				return out;
			}
		}
		return std::nullopt;
	}

	bool CarriesSpare(const Selection& a_item)
	{
		if (!Settings::bSpareRepair.GetValue() || !a_item.Worn()) {
			return false;
		}
		return std::ranges::any_of(Copies(a_item), [&a_item](const Copy& a_copy) {
			return SparesIn(a_copy) > 0 && !Refused(*a_item.object, a_copy.extra.get());
		});
	}

	std::vector<Spare> Spares(const Selection& a_item, const Priced& a_priced)
	{
		std::vector<Spare> out;
		if (!a_item.Worn()) {
			return out;
		}

		for (const auto& copy : Copies(a_item)) {
			const auto copies = SparesIn(copy);
			const auto spare = copies > 0 ? Worked(copy, a_item, a_priced) : std::nullopt;
			if (!spare) {
				continue;
			}
			TraceLog::Line("menu", "Workbench lists {:d} of {:s} at {:d}% as {:s} for {:s} at {:d}%: it scraps into {:s}, worth {:d}, for +{:d}%",
				copies, spare->name, spare->percent, copies == 1 ? "a spare" : "spares", a_item.Name(), a_item.percent,
				Spell(spare->scrap), spare->worth, spare->gain);
			out.insert(out.end(), std::min<std::size_t>(copies, MOST_SPARES), *spare);
		}

		// The spares players most likely want gone come first, and a list too
		// long for the box drops the ones that restore the most.
		std::ranges::stable_sort(out, [](const Spare& a_left, const Spare& a_right) {
			if (a_left.gain != a_right.gain) {
				return a_left.gain < a_right.gain;
			}
			return a_left.percent != a_right.percent ? a_left.percent < a_right.percent : a_left.name < a_right.name;
		});
		if (out.size() > MOST_SPARES) {
			out.erase(out.begin() + MOST_SPARES, out.end());
		}
		return out;
	}

	std::optional<Spare> SpareOn(const Selection& a_item, const Priced& a_priced, const RE::ExtraDataList* a_extra)
	{
		for (const auto& copy : Copies(a_item)) {
			if (copy.extra.get() == a_extra && SparesIn(copy) > 0) {
				return Worked(copy, a_item, a_priced);
			}
		}
		return std::nullopt;
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
		auto    owed = PriceOf(a_priced, a_item.percent, a_spare.gain);
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

		// What goes back is worked from the parts the spare holds, so a worth
		// above 1 never hands back more than the spare is made of.
		for (const auto& part : a_spare.held) {
			const auto taken = std::ranges::find(out.taken, part.component, &Materials::Part::component);
			const auto used = taken != out.taken.end() ? taken->count : 0U;
			if (part.count > used) {
				out.back.push_back({ part.component, part.count - used });
			}
		}
		return out;
	}

	std::string RowOf(const Spare& a_spare)
	{
		return Text::SpareRow(Text::SpareName(a_spare.name, a_spare.equipped, a_spare.favorite, a_spare.legendary),
			a_spare.percent, a_spare.gain);
	}
}
