#include "UI/Repair/VendorRepair/Restock.h"

#include "Core/TraceLog.h"
#include "UI/Repair/VendorRepair/Quote.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace VendorRepair
{
	namespace
	{
		using Flag = RE::TESLeveledList::Flag;

		// Merges a_more into a_into, each item's 2 chances rolled separately.
		void Either(Chances& a_into, const Chances& a_more)
		{
			for (const auto& [item, chance] : a_more) {
				auto& was = a_into[item];
				was = 1.0 - ((1.0 - was) * (1.0 - chance));
			}
		}

		// A list passes at or above its chance none, rolled from 0 to 99. The
		// chance can be a global a script lowers, a caravan's upgrade.
		[[nodiscard]] double Passes(RE::TESLevItem& a_list)
		{
			const auto none = static_cast<std::uint8_t>(a_list.GetChanceNone());
			return none >= 100 ? 0.0 : (100.0 - none) / 100.0;
		}

		// An entry passes above its own, so 99 never does, and 0 is not
		// rolled at all.
		[[nodiscard]] double Passes(const RE::LEVELED_OBJECT& a_entry)
		{
			const auto none = a_entry.chanceNone;
			if (none == 0) {
				return 1.0;
			}
			return none >= 99 ? 0.0 : (99.0 - none) / 100.0;
		}

		// Every entry of a_list as it stands, those a script added among them.
		[[nodiscard]] std::span<const RE::LEVELED_OBJECT> Entries(const RE::TESLevItem& a_list)
		{
			if (!a_list.leveledLists) {
				return {};
			}
			return { a_list.leveledLists, a_list.baseListCount };
		}

		// A chest's stock lists rolled at one level. Each list is worked out
		// once, since the vendor lists share their inner lists.
		class Roll
		{
		public:
			Roll(std::uint16_t a_level, std::uint16_t a_player) :
				level(a_level),
				pickLevel(std::min(a_level, a_player))
			{}

			// The chance of each item turning up when a_form is asked for
			// a_count times, as a stock line or a list's entry asks.
			[[nodiscard]] Chances Of(RE::TESForm& a_form, std::uint16_t a_count)
			{
				if (a_count == 0) {
					return {};
				}
				auto* list = a_form.As<RE::TESLevItem>();
				if (!list) {
					return Item(a_form, a_count);
				}

				// A list that calculates for each item in the count picks once
				// per count, and any other hands out the count of its pick.
				auto out = Once(*list);
				if (list->llFlags.all(Flag::kCalculateForEachItemInCount)) {
					for (auto& [item, chance] : out) {
						chance = 1.0 - std::pow(1.0 - chance, a_count);
					}
				}
				return out;
			}

		private:
			// An item that is no list. A gun brings its own ammunition, its
			// ammo list rolled for the gun's count, as in the game.
			[[nodiscard]] Chances Item(RE::TESForm& a_form, std::uint16_t a_count)
			{
				auto* item = a_form.As<RE::TESBoundObject>();
				if (!item) {
					return {};
				}
				Chances     out{ { item, 1.0 } };
				const auto* gun = item->As<RE::TESObjectWEAP>();
				if (gun && gun->weaponData.npcAddAmmoList) {
					Either(out, Of(*gun->weaponData.npcAddAmmoList, a_count));
				}
				return out;
			}

			// One call of a_list, or one pick of it for a list that calculates
			// for each item in the count, which comes first in the game too.
			[[nodiscard]] const Chances& Once(RE::TESLevItem& a_list)
			{
				if (const auto it = done.find(&a_list); it != done.end()) {
					return it->second;
				}

				// A list inside itself hands out nothing the second time round.
				static const Chances nothing;
				if (std::ranges::find(open, &a_list) != open.end()) {
					return nothing;
				}
				open.push_back(&a_list);
				auto out = a_list.llFlags.all(Flag::kUseAll) &&
				                   a_list.llFlags.none(Flag::kCalculateForEachItemInCount) ?
				               All(a_list) :
				               Pick(a_list);
				open.pop_back();
				return done.emplace(&a_list, std::move(out)).first->second;
			}

			// The list's chance none, then 1 entry at random, then that entry's
			// own chance none. The entries that can be picked are the top level
			// at or below the level. A list that calculates from all levels
			// takes every entry from its level difference below the level asked
			// up to the level, all of them where the difference is 0, and the
			// top level where that leaves none. The game only opens a level
			// above the last one, starting from 0, so an entry at level 0 is
			// never picked.
			[[nodiscard]] Chances Pick(RE::TESLevItem& a_list)
			{
				const auto    entries = Entries(a_list);
				std::uint16_t top = 0;
				for (const auto& entry : entries) {
					if (entry.level <= pickLevel) {
						top = std::max(top, entry.level);
					}
				}

				std::vector<const RE::LEVELED_OBJECT*> candidates;
				if (a_list.llFlags.all(Flag::kCalculateFromAllLevels)) {
					// iLevItemLevelDifferenceMax, as a plugin or setgs left it.
					const auto difference = a_list.GetMaxLevelDifference();
					const auto from = difference == 0 ? 1 : std::max(1, level - difference);
					for (const auto& entry : entries) {
						if (entry.level >= from && entry.level <= pickLevel) {
							candidates.push_back(&entry);
						}
					}
				}
				if (candidates.empty()) {
					for (const auto& entry : entries) {
						if (entry.level > 0 && entry.level == top) {
							candidates.push_back(&entry);
						}
					}
				}

				Chances out;
				if (candidates.empty()) {
					return out;
				}
				const auto each = Passes(a_list) / static_cast<double>(candidates.size());
				for (const auto* entry : candidates) {
					if (!entry->form) {
						continue;
					}
					const auto weight = each * Passes(*entry);
					for (const auto& [item, chance] : Of(*entry->form, entry->count)) {
						out[item] += weight * chance;
					}
				}
				return out;
			}

			// The list's chance none once, then every entry at or below the
			// level on its own chance none.
			[[nodiscard]] Chances All(RE::TESLevItem& a_list)
			{
				Chances out;
				for (const auto& entry : Entries(a_list)) {
					if (entry.level > level || !entry.form) {
						continue;
					}
					auto       one = Of(*entry.form, entry.count);
					const auto passes = Passes(entry);
					for (auto& [item, chance] : one) {
						chance *= passes;
					}
					Either(out, one);
				}
				const auto passes = Passes(a_list);
				for (auto& [item, chance] : out) {
					chance *= passes;
				}
				return out;
			}

			std::uint16_t                                level;
			std::uint16_t                                pickLevel;
			std::unordered_map<RE::TESLevItem*, Chances> done;
			std::vector<RE::TESLevItem*>                 open;
		};

		// A chest behind a trader, and how it is theirs, for the trace log.
		struct Chest
		{
			RE::NiPointer<RE::TESObjectREFR> ref;
			std::string                      how;
		};

		// What the trace log calls a_chest: the reference, its base and how
		// it is the trader's.
		[[nodiscard]] std::string Which(const Chest& a_chest)
		{
			const auto* base = a_chest.ref->GetObjectReference();
			return std::format("Chest {:08X} of {:08X}, {:s}", a_chest.ref->formID, base ? base->formID : 0,
				a_chest.how);
		}

		// The stock list a_chest restocks from, or nothing for a chest a
		// script fills.
		[[nodiscard]] const RE::TESObjectCONT* StockList(RE::TESObjectREFR& a_chest)
		{
			const auto* base = a_chest.GetObjectReference();
			const auto* chest = base ? base->As<RE::TESObjectCONT>() : nullptr;
			return chest && chest->numContainerObjects > 0 ? chest : nullptr;
		}

		// What a_chest restocks with, its stock list rolled at the chest's
		// level. The game asks a leveled line for its count without the sign,
		// cut to 16 bits, and a plain gun's ammo list for 1. A plain line of 0
		// or less is left out, as the game leaves it out for an item built
		// from a template, most weapons and armor. The game makes a 0 of
		// anything else 1, which no vanilla chest has.
		[[nodiscard]] Chances Rolled(const Chest& a_chest, const RE::TESObjectCONT& a_list, std::uint16_t a_player)
		{
			const auto level = a_chest.ref->GetCalcLevel(false);
			Roll       roll{ level, a_player };
			Chances    out;
			a_list.ForEachContainerObject([&](RE::ContainerObject& a_line) {
				if (a_line.obj && a_line.obj->Is(RE::ENUM_FORM_ID::kLVLI)) {
					const auto count = std::abs(static_cast<std::int64_t>(a_line.count));
					Either(out, roll.Of(*a_line.obj, static_cast<std::uint16_t>(count)));
				} else if (a_line.obj && a_line.count > 0) {
					Either(out, roll.Of(*a_line.obj, 1));
				}
				return true;
			});
			TraceLog::Line("menu", "{:s}, rolls at level {:d} and can hand out {:d} items", Which(a_chest), level,
				out.size());
			return out;
		}

		// What a_chest holds now, each item once.
		[[nodiscard]] Chances AsItStands(const Chest& a_chest)
		{
			Chances out;
			if (auto* inv = a_chest.ref->inventoryList) {
				const RE::BSAutoReadLock l(inv->rwLock);
				for (const auto& item : inv->data) {
					if (item.object) {
						out[item.object] = 1.0;
					}
				}
			}
			TraceLog::Line("menu", "{:s}, is filled by a script and holds {:d} items", Which(a_chest), out.size());
			return out;
		}

		// The chests behind a_trader: the faction's merchant container, which
		// the screen names, then the ones linked to them by a keyword of
		// VendorKeywordLinkedRefFormList, the game's default object for a
		// settlement store. The screen walks the list's own forms and none a
		// script added.
		[[nodiscard]] std::vector<Chest> Chests(RE::TESObjectREFR* a_merchant, RE::Actor& a_trader)
		{
			std::vector<Chest> out;
			const auto add = [&](RE::TESObjectREFR* a_chest, std::string a_how) {
				const auto known = std::ranges::any_of(out, [a_chest](const Chest& a_known) {
					return a_known.ref.get() == a_chest;
				});
				if (a_chest && a_chest != &a_trader && !known) {
					out.push_back({ RE::NiPointer<RE::TESObjectREFR>{ a_chest }, std::move(a_how) });
				}
			};
			add(a_merchant, "the merchant container");

			auto*       store = RE::TESForm::GetFormByEditorID<RE::BGSDefaultObject>("VendorKeywordLinkedRefFormList");
			const auto* keywords = store ? store->GetForm<RE::BGSListForm>() : nullptr;
			if (keywords) {
				for (auto* form : keywords->arrayOfForms) {
					auto* keyword = form ? form->As<RE::BGSKeyword>() : nullptr;
					if (auto* chest = keyword ? a_trader.GetLinkedRef(keyword) : nullptr) {
						add(chest, std::format("linked by {:s} [{:08X}]", keyword->formEditorID.c_str(), keyword->formID));
					}
				}
			}
			return out;
		}

		// Whether the screen shows a_item on a_trader's side: never caps, an
		// unnamed or a non playable item, and only what the trader deals in,
		// the rule the screen itself asks.
		[[nodiscard]] bool OnShelf(RE::TESBoundObject& a_item, RE::Actor& a_trader, const RE::TESBoundObject* a_caps)
		{
			return &a_item != a_caps && !RE::TESFullName::GetFullName(a_item).empty() &&
			       a_item.GetPlayable(nullptr) && a_trader.CanBarterItem(&a_item, false, false, false, false);
		}
	}

	Chances Restock(RE::TESObjectREFR* a_merchant, RE::Actor& a_trader)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return {};
		}

		const auto level = static_cast<std::uint16_t>(std::max<std::int16_t>(player->GetLevel(), 1));
		const auto chests = Chests(a_merchant, a_trader);
		Chances    out;
		bool       listed = false;
		for (const auto& chest : chests) {
			if (const auto* list = StockList(*chest.ref)) {
				Either(out, Rolled(chest, *list, level));
				listed = true;
			}
		}

		// A chest a script fills counts only behind a trader who has nothing
		// else, see Restock.h.
		for (const auto& chest : chests) {
			if (StockList(*chest.ref)) {
				continue;
			}
			if (listed) {
				TraceLog::Line("menu", "{:s}, is filled by a script and left out", Which(chest));
			} else {
				Either(out, AsItStands(chest));
			}
		}

		const auto* caps = Caps();
		std::erase_if(out, [&](const auto& a_item) {
			return a_item.second <= 0.0 || !OnShelf(*a_item.first, a_trader, caps);
		});
		return out;
	}
}
