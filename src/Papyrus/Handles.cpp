#include "Papyrus/Items.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Condition/UniqueID.h"
#include "Core/ScriptEvents.h"
#include "Core/TraceLog.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "Papyrus/Natives.h"
#include "Papyrus/Numbering.h"

#include <cstdint>
#include <format>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// Handing a script 1 handle per stack, numbered by HandlesFor in
// Numbering.h. A game where no mod calls NEC gets no number from it.
namespace Papyrus
{
	namespace
	{
		// -------------------------------------------------------------------
		// The stacks a function hands out
		// -------------------------------------------------------------------

		// a_stack as a Target, or nothing for a stack with no list, which has
		// no place for a number. The engine's write would also crash on it,
		// see Equipped::EquippedStack.
		std::optional<Target> TargetOf(RE::TESBoundObject* a_base, const RE::BGSInventoryItem::Stack& a_stack)
		{
			auto* extra = a_stack.extra.get();
			if (!extra) {
				return std::nullopt;
			}
			Target target{ a_base, extra, UniqueID::Of(extra) };
			if (const auto* handles = extra->GetByType<RE::ExtraReferenceHandles>()) {
				if (const auto ref = handles->originalRef.get()) {
					target.ref = ref->GetFormID();
				}
			}
			return target;
		}

		// Every stack of a_base, or with a_base null of every weapon and armor
		// NEC gives a condition and every food that spoils, in list order.
		auto StacksOf(RE::TESBoundObject* a_base)
		{
			return [a_base](RE::Actor*, const RE::BGSInventoryList& a_inv, std::vector<Target>& a_out) {
				for (const auto& entry : a_inv.data) {
					auto* object = entry.object;
					if (!object || (a_base ? object != a_base : !Condition::WearsOut(*object) && !FoodSpoil::Spoils(*object))) {
						continue;
					}
					for (auto* stack = entry.stackData.get(); stack; stack = stack->nextStack.get()) {
						if (const auto target = TargetOf(object, *stack)) {
							a_out.push_back(*target);
						}
					}
				}
			};
		}

		// The stack whose list is a_list, a pointer compare. The caller holds
		// the list, so its address names no other list.
		auto ByList(const RE::ExtraDataList* a_list)
		{
			return [a_list](RE::Actor*, const RE::BGSInventoryList& a_inv, std::vector<Target>& a_out) {
				for (const auto& entry : a_inv.data) {
					for (auto* stack = entry.stackData.get(); stack; stack = stack->nextStack.get()) {
						if (stack->extra.get() == a_list) {
							if (const auto target = TargetOf(entry.object, *stack)) {
								a_out.push_back(*target);
							}
							return;
						}
					}
				}
			};
		}

		// The first equipped stack of a_base.
		auto FirstEquipped(const RE::TESBoundObject& a_base)
		{
			return [&a_base](RE::Actor*, const RE::BGSInventoryList& a_inv, std::vector<Target>& a_out) {
				for (const auto& entry : a_inv.data) {
					if (entry.object != &a_base) {
						continue;
					}
					for (auto* stack = entry.stackData.get(); stack; stack = stack->nextStack.get()) {
						if (stack->IsEquipped()) {
							if (const auto target = TargetOf(entry.object, *stack)) {
								a_out.push_back(*target);
							}
							return;
						}
					}
				}
			};
		}

		// The equipped stack of the weapon in hand, as Equipped::Weapon finds
		// the weapon. Its walk takes the read lock inside this write lock.
		void HeldWeapon(RE::Actor* a_actor, const RE::BGSInventoryList& a_inv, std::vector<Target>& a_out)
		{
			auto* weapon = Equipped::Weapon(a_actor);
			for (const auto& entry : a_inv.data) {
				if (!weapon || entry.object != weapon) {
					continue;
				}
				for (auto* stack = entry.stackData.get(); stack; stack = stack->nextStack.get()) {
					if (stack->IsEquipped()) {
						if (const auto target = TargetOf(entry.object, *stack)) {
							a_out.push_back(*target);
							return;
						}
					}
				}
			}
		}

		// Every equipped stack of armor NEC gives a condition, on any of
		// a_slots, bit 0 for slot 30.
		auto WornOn(std::uint32_t a_slots, bool a_first)
		{
			return [a_slots, a_first](RE::Actor*, const RE::BGSInventoryList& a_inv, std::vector<Target>& a_out) {
				for (const auto& entry : a_inv.data) {
					auto* object = entry.object;
					if (!object || !object->Is(RE::ENUM_FORM_ID::kARMO) || !Condition::WearsOut(*object) ||
						(static_cast<const RE::TESObjectARMO*>(object)->bipedModelData.bipedObjectSlots & a_slots) == 0) {
						continue;
					}
					for (auto* stack = entry.stackData.get(); stack; stack = stack->nextStack.get()) {
						if (stack->IsEquipped()) {
							if (const auto target = TargetOf(object, *stack)) {
								a_out.push_back(*target);
								if (a_first) {
									return;
								}
							}
						}
					}
				}
			};
		}

		// HandlesFor for a script's call, which answers nothing while the
		// game loads or froze its scripts, see ScriptEvents.h.
		std::optional<std::vector<Item>> ForScript(RE::TESObjectREFR& a_holder, const Pick& a_pick)
		{
			if (!Answering()) {
				return std::nullopt;
			}
			Hold       hold;
			const auto items = HandlesFor(a_holder, a_pick, hold);
			ScriptEvents::KickIfWaiting();
			return items;
		}

		[[nodiscard]] std::string ShownItems(const std::vector<Item>& a_items)
		{
			std::string text = std::format("{:d} items", a_items.size());
			for (std::size_t i = 0; i < a_items.size(); ++i) {
				std::format_to(std::back_inserter(text), "{:s} {:s}", i == 0 ? ":" : ",", Shown(a_items[i]));
			}
			return text;
		}

		Item FirstOf(const std::optional<std::vector<Item>>& a_items)
		{
			const auto item = a_items && !a_items->empty() ? a_items->front() : Item{};
			TraceLog::Papyrus::Line("result", "{:s}", Shown(item));
			return item;
		}

		std::optional<std::vector<Item>> AllOf(std::optional<std::vector<Item>> a_items)
		{
			TraceLog::Papyrus::Line("result", "{:s}", a_items ? ShownItems(*a_items) : "None");
			return a_items;
		}
	}

	// -------------------------------------------------------------------
	// The 4 functions
	// -------------------------------------------------------------------

	std::optional<std::vector<Item>> GetItems(std::monostate, RE::TESObjectREFR* a_holder, RE::TESForm* a_base)
	{
		Log::Calling("GetItems");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetItems holder={:s} base={:s}", Hex(a_holder), Hex(a_base));
		// A reference passed as the base names no item.
		auto* base = a_base && a_base->IsBoundObject() ? static_cast<RE::TESBoundObject*>(a_base) : nullptr;
		if (!a_holder || (a_base && !base)) {
			return AllOf(std::nullopt);
		}
		return AllOf(ForScript(*a_holder, StacksOf(base)));
	}

	Item GetEquippedWeapon(std::monostate, RE::Actor* a_actor)
	{
		Log::Calling("GetEquippedWeapon");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetEquippedWeapon actor={:s}", Hex(a_actor));
		return FirstOf(a_actor ? ForScript(*a_actor, HeldWeapon) : std::nullopt);
	}

	// Power armor pieces are the engine's, as Equipped::ArmorPieces says, so
	// an actor in a suit wears none NEC tracks.
	Item GetWornItem(std::monostate, RE::Actor* a_actor, std::int32_t a_slot)
	{
		Log::Calling("GetWornItem");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetWornItem actor={:s} slot={:d}", Hex(a_actor), a_slot);
		if (!a_actor || a_slot < 30 || a_slot > 61 || RE::PowerArmor::ActorInPowerArmor(*a_actor)) {
			return FirstOf(std::nullopt);
		}
		return FirstOf(ForScript(*a_actor, WornOn(1u << (a_slot - 30), true)));
	}

	std::optional<std::vector<Item>> GetWornItems(std::monostate, RE::Actor* a_actor)
	{
		Log::Calling("GetWornItems");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetWornItems actor={:s}", Hex(a_actor));
		if (!a_actor) {
			return AllOf(std::nullopt);
		}
		if (RE::PowerArmor::ActorInPowerArmor(*a_actor)) {
			return AllOf(std::vector<Item>{});
		}
		return AllOf(ForScript(*a_actor, WornOn(~0u, false)));
	}

	bool NameStack(RE::TESObjectREFR& a_holder, const RE::ExtraDataList* a_list, const RE::TESBoundObject& a_base,
		const std::function<bool(const Item&)>& a_inside)
	{
		Hold       hold{ .once = true, .inside = &a_inside };
		const auto items = a_list ? HandlesFor(a_holder, ByList(a_list), hold) : HandlesFor(a_holder, FirstEquipped(a_base), hold);
		if (hold.busy) {
			return false;
		}
		if (!items || items->empty()) {
			a_inside(Item{});
		}
		return true;
	}
}
