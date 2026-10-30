#include "Gameplay/FoodSpoil/Food.h"

#include "Core/CallPatch/CallPatch.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "Gameplay/FoodSpoil/Kinds.h"
#include "Gameplay/FoodSpoil/Stamp.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <vector>

namespace FoodSpoil
{
	namespace
	{
		// -------------------------------------------------------------------
		// The station
		// -------------------------------------------------------------------

		// Where a bench copies everything it reaches into the 1 list its rows
		// count from: as it opens and after each build. OG's open goes
		// through the 2nd call.
		constexpr CallPatch::CallSite COOK_SITES[] = {
			{ CallPatch::AE{ RE::ID::WorkbenchMenuBase::ProcessMessage.ae(), 0x318 }, CallPatch::NO_OG_COVERED, "cook mark open" },
			{ CallPatch::AE{ RE::ID::WorkbenchMenuBase::UpdateOptimizedAutoBuildInv.ae(), 0x00D }, CallPatch::OG{ RE::ID::WorkbenchMenuBase::UpdateOptimizedAutoBuildInv.og(), 0x00D }, "cook mark" },
		};

		// Where a build takes each ingredient from the bench's containers.
		constexpr CallPatch::CallSite TAKE_SITE{ CallPatch::AE{ RE::ID::ExamineMenu::ConsumeSelectedItems.ae(), 0x0BA }, CallPatch::OG{ RE::ID::ExamineMenu::ConsumeSelectedItems.og(), 0x0CA }, "fresh take" };

		using Build_t = std::uintptr_t (*)(RE::WorkbenchMenuBase*);
		using Take_t = void (*)(RE::WorkbenchMenuBase*, RE::TESBoundObject*, std::uint32_t);

		CallPatch::Held                                             g_cook;
		std::array<CallPatch::Link<Build_t>, std::size(COOK_SITES)> g_buildLinks;
		CallPatch::Link<Take_t>                                     g_takeLink;

		// The station whose list is filling, on the thread that fills it.
		thread_local const RE::WorkbenchMenuBase* t_station = nullptr;

		// Whether a_menu is a cooking or a chemistry station's.
		bool IsFoodStation(const RE::WorkbenchMenuBase& a_menu)
		{
			return StationHas(a_menu, COOKING) || StationHas(a_menu, CHEMISTRY);
		}

		// -------------------------------------------------------------------
		// Stacks in reach
		// -------------------------------------------------------------------

		// Whether a_holder's inventory has the stack a_extra belongs to.
		bool Holds(const RE::TESObjectREFR& a_holder, const RE::ExtraDataList* a_extra)
		{
			auto* inv = a_holder.inventoryList;
			if (!inv) {
				return false;
			}
			const RE::BSAutoReadLock l(inv->rwLock);
			for (const auto& item : inv->data) {
				for (const auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
					if (stack->extra.get() == a_extra) {
						return true;
					}
				}
			}
			return false;
		}

		// Whether a_extra's stack, copied from a container a_station reaches,
		// lies in a fridge. The bench and its workshop are known before the
		// copies start, and so are the supply lines' containers. The player
		// is never a fridge.
		bool InStationFridge(const RE::WorkbenchMenuBase& a_station, const RE::ExtraDataList* a_extra)
		{
			const auto inFridge = [&](const RE::TESObjectREFR* a_ref) {
				return IsFridgeRef(a_ref) && Holds(*a_ref, a_extra);
			};
			return inFridge(a_station.workbenchRef.get()) || inFridge(a_station.workbenchContainerRef.get()) ||
			       std::ranges::any_of(a_station.sharedContainers, [&](const auto& a_ref) { return inFridge(a_ref.get()); });
		}

		// The ID a_extra's stack of a_food has in a_holder now. IDs are places
		// in the chain, so they shift as stacks before it go.
		std::optional<std::uint32_t> StackID(const RE::TESObjectREFR& a_holder, const RE::TESBoundObject& a_food, const RE::ExtraDataList* a_extra)
		{
			auto* inv = a_holder.inventoryList;
			if (!inv) {
				return std::nullopt;
			}
			const RE::BSAutoReadLock l(inv->rwLock);
			for (const auto& item : inv->data) {
				if (item.object != &a_food) {
					continue;
				}
				std::uint32_t id = 0;
				for (const auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get(), id++) {
					if (stack->extra.get() == a_extra) {
						return id;
					}
				}
			}
			return std::nullopt;
		}

		// -------------------------------------------------------------------
		// The hooks
		// -------------------------------------------------------------------

		// The bench copies what it reaches into its list. At a food station
		// the mark lets the door leave spoiled food out, see LeftOut.
		template <std::size_t I>
		std::uintptr_t BuildHk(RE::WorkbenchMenuBase* a_menu)
		{
			if (!g_cook.Runs(g_buildLinks[I]) || !a_menu || !IsFoodStation(*a_menu)) {
				return g_buildLinks[I](a_menu);
			}
			t_station = a_menu;
			const auto result = g_buildLinks[I](a_menu);
			t_station = nullptr;
			return result;
		}

		// A build takes a_count of a_object. Food that spoils comes only from
		// the fresh stacks the rows counted, the one closest to spoiling
		// first. The vanilla take starts at each container's first stack,
		// which may be spoiled.
		void TakeHk(RE::WorkbenchMenuBase* a_menu, RE::TESBoundObject* a_object, std::uint32_t a_count)
		{
			const auto today = Today();
			if (!g_cook.Runs(g_takeLink) || !a_menu || !a_object || !today || !Spoils(*a_object) || !IsFoodStation(*a_menu)) {
				g_takeLink(a_menu, a_object, a_count);
				return;
			}
			auto stacks = StationStacks(*a_menu, a_object, true, *today);
			std::ranges::stable_sort(stacks, {}, &HeldStack::freshness);
			for (const auto& stack : stacks) {
				if (a_count == 0) {
					break;
				}
				a_count -= Take(stack, a_count);
			}
		}
	}

	bool StationHas(const RE::WorkbenchMenuBase& a_menu, const char* a_editorID)
	{
		if (*reinterpret_cast<const std::uintptr_t*>(&a_menu) != RE::VTABLE::CookingMenu[0].address()) {
			return false;
		}
		const auto* bench = a_menu.workbenchRef.get();
		const auto* base = bench ? bench->GetObjectReference() : nullptr;
		const auto* furniture = base ? base->As<RE::TESFurniture>() : nullptr;
		const auto* keyword = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>(a_editorID);
		return furniture && keyword && furniture->HasKeyword(keyword, nullptr);
	}

	std::vector<HeldStack> StationStacks(const RE::WorkbenchMenuBase& a_station, const RE::TESBoundObject* a_food, bool a_fresh, float a_today)
	{
		std::vector<HeldStack> stacks;
		for (const auto& holder : a_station.sharedContainers) {
			auto* inv = holder ? holder->inventoryList : nullptr;
			if (!inv) {
				continue;
			}
			const auto               fridge = IsFridgeRef(holder.get());
			const RE::BSAutoReadLock l(inv->rwLock);
			for (const auto& item : inv->data) {
				if (!item.object || (a_food && item.object != a_food) || !Spoils(*item.object)) {
					continue;
				}
				const auto kind = KindOf(*item.object);
				for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
					const auto freshness = FreshnessIn(kind, stack->extra.get(), fridge, a_today);
					if (stack->count > 0 && (freshness > 0.0F) == a_fresh) {
						stacks.push_back({ holder, item.object, stack->extra, stack->count, freshness });
					}
				}
			}
		}
		return stacks;
	}

	std::uint32_t Take(const HeldStack& a_stack, std::uint32_t a_most)
	{
		const auto id = StackID(*a_stack.holder, *a_stack.food, a_stack.extra.get());
		if (!id) {
			return 0;
		}
		const auto                        taken = std::min(a_most, a_stack.count);
		RE::TESObjectREFR::RemoveItemData take{ a_stack.food, static_cast<std::int32_t>(taken) };
		take.stackData.push_back(*id);
		a_stack.holder->RemoveItem(take);
		return taken;
	}

	bool LeftOut(const RE::BGSInventoryList& a_list, const RE::TESBoundObject& a_object, const RE::BGSInventoryItem::Stack& a_stack)
	{
		const auto* station = t_station;
		const auto* copy = station ? station->sharedContainerRef.get() : nullptr;
		if (!copy || copy->inventoryList != &a_list || !Spoils(a_object)) {
			return false;
		}
		auto*      extra = a_stack.extra.get();
		const auto stamp = StampOf(extra);
		const auto today = Today();
		if (!stamp || !today) {
			return IsSpoiled(extra);
		}
		const auto kind = KindOf(a_object);
		if (Freshness(kind, *stamp, *today) > 0.0F) {
			return false;
		}
		// Food in a fridge has an earlier stamp, see Food.h, so it may still
		// be fresh there.
		return !InStationFridge(*station, extra) || Freshness(kind, Thaw(*stamp, *today), *today) <= 0.0F;
	}

	void InstallCook(Part a_group)
	{
		// All or none: a mark with no fresh take would let a build take the
		// spoiled pieces the rows left out.
		const CallPatch::Together cook{ a_group };
		const auto                builds = CallPatch::PerSite<std::size(COOK_SITES)>([]<std::size_t I>() { return &BuildHk<I>; });
		CallPatch::PatchAll(COOK_SITES, RE::ID::WorkbenchMenuBase::BuildSharedContainers, builds, g_buildLinks, "Cooking and chemistry stations count only fresh food");
		CallPatch::PatchCall(TAKE_SITE, RE::ID::WorkbenchMenuBase::RemoveObjectFromContainers, reinterpret_cast<std::uintptr_t>(&TakeHk), g_takeLink);
		g_cook = cook.Set();
	}
}
