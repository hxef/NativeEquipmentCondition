#include "Gameplay/ArmorRating.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ArmorRating
{
	namespace
	{
		using CallPatch::CallSite;

		// The calls to the armor rating visitor, one per equipped piece.
		// CalcArmorRating1 adds up the actor's own DamageResist. CalcArmorRating2
		// rates one item for the menus, and the AI rates a piece it might pick
		// up or put on through the other 2, so everybody sees the same worn
		// number. The engine's RecalcArmorRating holds the 6th call and has
		// no callers.
		constexpr CallSite VISITOR_SITES[] = {
			{ RE::ID::Actor::CalcArmorRating1.id(), 0x0FC, "damage resist" },
			{ RE::ID::Actor::CalcArmorRating2.id(), 0x02B, "one piece rating" },
			{ RE::ID::Actor::GetDesirability.id(), 0x056, "AI desirability" },
			{ RE::ID::CombatBehaviorFindObject::EvaluateArmor.id(), 0x4ED, "AI evaluate armor" },
			{ RE::ID::CombatBehaviorFindObject::EvaluateArmor.id(), 0x579, "AI evaluate armor again" },
		};

		// The one call to SumEquippedArmorDamageTypes, inside
		// UpdateEquippedArmorResistances, which turns the sums into the typed
		// resistance actor values.
		constexpr CallSite SUM_SITE{ RE::ID::ActorUtils::UpdateEquippedArmorResistances.id(), 0x119, "typed resistances" };

		// The calls to FillResistTypeInfo, which adds one copy's resistances
		// into the list a menu prints: the menus' item card, once for the item
		// and once for the equipped piece it is held against, the Pip-Boy's
		// card, and the paper doll, once per equipped piece. The same function
		// serves the better mark and the sort, which are left as they are.
		constexpr CallSite FILL_SITES[] = {
			{ RE::ID::InventoryUserUIUtils::PopulateItemCardInfo_Helper.id(), 0xEE9, "card resistances" },
			{ RE::ID::InventoryUserUIUtils::PopulateItemCardInfo_Helper.id(), 0xFB2, "card resistances compared" },
			{ RE::ID::PipboyInventoryData::PopulateItemCardInfo.id(), 0x74A, "pipboy card resistances" },
			{ RE::ID::PipboyInventoryData::UpdateSlotResists.id(), 0x159, "paper doll resistances" },
		};

		using Visitor_t = std::int64_t (*)(RE::ActorUtils::ArmorRatingVisitorBase*, const RE::BGSInventoryItem*, std::uint32_t);
		using Sum_t = bool (*)(const RE::BSTArray<RE::BGSInventoryItem>&, RE::BSTScrapHashMap<RE::BGSDamageType*, std::int32_t>* const&);
		using Fill_t = void (*)(const RE::BGSInventoryItem&, const RE::BGSInventoryItem::Stack*,
			RE::BSScrapArray<RE::BSTTuple<std::uint32_t, float>>&, float);

		std::array<CallPatch::Link<Visitor_t>, std::size(VISITOR_SITES)> g_visitorLinks;
		CallPatch::Link<Sum_t>                                          g_sumLink;
		std::array<CallPatch::Link<Fill_t>, std::size(FILL_SITES)>       g_fillLinks;

		// The stack the visitor read: the equipped one for ANY_STACK with the
		// check on, the first otherwise, or the one named.
		const RE::BGSInventoryItem::Stack* StackRead(const RE::ActorUtils::ArmorRatingVisitorBase& a_visitor,
			const RE::BGSInventoryItem& a_item, std::uint32_t a_stackID)
		{
			if (a_stackID != Equipped::ANY_STACK) {
				return a_item.GetStackByID(a_stackID);
			}
			for (auto* stack = a_item.stackData.get(); stack; stack = stack->nextStack.get()) {
				if (!a_visitor.checkEquipped || stack->IsEquipped()) {
					return stack;
				}
			}
			return nullptr;
		}

		// Stands in for the visitor's operator(). The visitor adds the piece's
		// rating, through the perk entry point and rounded up, to its total,
		// and this scales what it added.
		template <std::size_t I>
		std::int64_t VisitorHk(RE::ActorUtils::ArmorRatingVisitorBase* a_visitor, const RE::BGSInventoryItem* a_item, std::uint32_t a_stackID)
		{
			const auto before = a_visitor->rating;
			const auto result = g_visitorLinks[I](a_visitor, a_item, a_stackID);
			const auto added = a_visitor->rating - before;
			if (!g_visitorLinks[I].Live() || added <= 0.0F || !a_item || !a_item->object || !Condition::WearsOut(*a_item->object)) {
				return result;
			}

			const auto health = Condition::HealthOf(StackRead(*a_visitor, *a_item, a_stackID));
			const auto share = Share(health);
			if (share >= 1.0F) {
				return result;
			}
			a_visitor->rating = before + added * share;

			// The player's pieces only, and once per health value, since the
			// visitor runs on every read of DamageResist.
			if (a_visitor->actor == RE::PlayerCharacter::GetSingleton()) {
				TraceLog::Once("armor", "{:s} [{:08X}] at {:.3f} counts {:.1f} of its {:.1f} rating",
					RE::TESFullName::GetFullName(*a_item->object), a_item->object->formID, health, added * share, added);
			}
			return result;
		}

		// Stands in for SumEquippedArmorDamageTypes. The engine's sum reads the
		// first stack of each piece, and this takes off what each equipped copy
		// has lost, so a worn piece's energy and radiation resistance fall with
		// its rating.
		bool SumHk(const RE::BSTArray<RE::BGSInventoryItem>& a_items, RE::BSTScrapHashMap<RE::BGSDamageType*, std::int32_t>* const& a_sums)
		{
			const auto result = g_sumLink(a_items, a_sums);
			if (!g_sumLink.Live()) {
				return result;
			}

			// What the wear took off each type, added up over the pieces.
			std::unordered_map<const RE::BGSDamageType*, float> lost;
			for (const auto& item : a_items) {
				if (!item.object || !item.object->Is(RE::ENUM_FORM_ID::kARMO) || !Condition::WearsOut(*item.object)) {
					continue;
				}
				const auto& armor = static_cast<const RE::TESObjectARMO&>(*item.object);
				for (const auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
					if (!stack->IsEquipped()) {
						continue;
					}
					const auto  share = Share(Condition::HealthOf(stack));
					// The copy's own damage types where it carries some, the
					// record's otherwise, as the engine's sum reads them.
					const auto* types = share < 1.0F ?
					                        Condition::InstanceDataOf(stack->extra.get(), armor.armorData).damageTypes :
					                        nullptr;
					if (!types) {
						continue;
					}
					for (const auto& entry : *types) {
						auto* type = entry.first ? entry.first->As<RE::BGSDamageType>() : nullptr;
						if (type && entry.second.i > 0) {
							lost[type] += static_cast<float>(entry.second.i) * (1.0F - share);
						}
					}
				}
			}
			if (lost.empty()) {
				return result;
			}

			// Only the player's own pieces are logged. Everyone else changes
			// clothes as they spawn, which would flood the log.
			const auto* player = RE::PlayerCharacter::GetSingleton();
			const bool  mine = player && player->inventoryList && &player->inventoryList->data == &a_items;
			for (auto& entry : *a_sums) {
				const auto found = lost.find(entry.first);
				if (found == lost.end()) {
					continue;
				}
				const auto taken = static_cast<std::int32_t>(std::lround(found->second));
				entry.second = std::max(0, entry.second - taken);
				if (mine) {
					TraceLog::Once("armor", "{:s} [{:08X}] resistance {:d} after {:d} lost to wear",
						RE::TESFullName::GetFullName(*entry.first), entry.first->formID, entry.second, taken);
				}
			}
			return result;
		}

		// Stands in for FillResistTypeInfo. Each resistance read before the
		// copy was added in is kept, so the copy's own part can be scaled and
		// the rest left alone: the paper doll adds a region's pieces into one
		// list, and the card holds the piece against the equipped one.
		template <std::size_t I>
		void FillHk(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack,
			RE::BSScrapArray<RE::BSTTuple<std::uint32_t, float>>& a_values, float a_scale)
		{
			const auto share = g_fillLinks[I].Live() && a_item.object && Condition::WearsOut(*a_item.object) ? Share(Condition::HealthOf(a_stack)) : 1.0F;
			if (share >= 1.0F) {
				g_fillLinks[I](a_item, a_stack, a_values, a_scale);
				return;
			}

			std::vector<float> before;
			before.reserve(a_values.size());
			for (const auto& entry : a_values) {
				before.push_back(entry.second);
			}

			g_fillLinks[I](a_item, a_stack, a_values, a_scale);

			for (std::size_t i = 0; i < a_values.size(); i++) {
				const auto was = i < before.size() ? before[i] : 0.0F;
				a_values[i].second = was + (a_values[i].second - was) * share;
			}

			TraceLog::First("menu", "{:s} [{:08X}] at {:.3f} prints {:.2f} of its resistances",
				RE::TESFullName::GetFullName(*a_item.object), a_item.object->formID, Condition::HealthOf(a_stack), share);
		}

		// fArmorFloor, kept between 0 and 1. Below 0 a worn piece would take
		// resistance away from the other pieces.
		float Floor()
		{
			const auto floor = Settings::fArmorFloor.GetValue();
			return floor > 0.0F ? std::min(floor, 1.0F) : 0.0F;
		}
	}

	float Share(float a_health)
	{
		return Condition::Share(a_health, Floor());
	}

	void Refresh(RE::Actor& a_actor)
	{
		RE::ActorUtils::UpdateEquippedArmorResistances(&a_actor);
	}

	void Install()
	{
		const auto visitorHooks = CallPatch::PerSite<std::size(VISITOR_SITES)>([]<std::size_t I>() { return &VisitorHk<I>; });
		CallPatch::PatchAll(VISITOR_SITES, RE::ID::ActorUtils::ArmorRatingVisitorBase::_operator, visitorHooks, g_visitorLinks,
			"A worn piece of armor counts for less of its rating");

		if (CallPatch::PatchCall(SUM_SITE, RE::ID::ActorUtils::SumEquippedArmorDamageTypes.address(),
				reinterpret_cast<std::uintptr_t>(&SumHk), g_sumLink)) {
			REX::INFO("A worn piece of armor counts for less of its energy and radiation resistance.");
		} else {
			REX::ERROR("A worn piece of armor will keep its full energy and radiation resistance.");
		}

		const auto fillHooks = CallPatch::PerSite<std::size(FILL_SITES)>([]<std::size_t I>() { return &FillHk<I>; });
		CallPatch::PatchAll(FILL_SITES, RE::ID::PipboyInventoryUtils::FillResistTypeInfo, fillHooks, g_fillLinks,
			"Item cards and the paper doll print a worn piece's resistances", Part::kCardArmor);

		REX::INFO("A piece of armor at nothing protects for {:.0f}% of its resistances.", Floor() * 100.0F);
	}
}
