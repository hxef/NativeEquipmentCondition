#include "Gameplay/BrokenEquip.h"

#include "Core/CallPatch.h"
#include "Core/TraceLog.h"

#include <optional>
#include <utility>

namespace BrokenEquip
{
	namespace
	{
		using Result = RE::ActorEquipManager::CanEquipResult;

		// The 2 calls to ActorEquipManager::CanEquip: the toggle's, before it
		// equips or takes off, and the container screen's, before it shows
		// its equip button.
		constexpr CallPatch::CallSite TOGGLE_SITE{ RE::ID::ActorEquipManager::ToggleEquipInventoryItem.id(), 0x188, "equip check" };
		constexpr CallPatch::CallSite BUTTON_SITE{ RE::ID::ContainerMenu::GetCanEquipItem.id(), 0x19C, "equip button" };

		// The toggle for each item the game took off for a while and now puts
		// back.
		constexpr CallPatch::CallSite PUT_BACK_SITE{ RE::ID::Actor::EquipShouldEquipItems.id(), 0xDE, "put back" };

		// The item the game is putting back, while it does. The question is
		// asked inside the toggle on the same thread, about the same handle.
		thread_local std::optional<std::uint32_t> t_puttingBack;

		Result CanEquipHk(RE::ActorEquipManager* a_manager, RE::Actor* a_actor, const std::uint32_t& a_handleID, std::uint32_t a_stackID)
		{
			// The patch replaced the call, not the function, so this does not
			// recurse.
			const auto result = a_manager->CanEquip(a_actor, a_handleID, a_stackID);

			// A yes, a locked item and the broken refusal are decided here.
			// Every other result refuses the item for its own reason and is
			// kept.
			if (result != Result::kSuccess && result != Result::kEquipStateLocked && result != Result::kItemBroken) {
				return result;
			}

			// The stack the question was about, found the way the game found
			// it.
			const auto* inventory = RE::BGSInventoryInterface::GetSingleton();
			const auto* item = inventory ? inventory->RequestInventoryItem(a_handleID) : nullptr;
			const auto* stack = item ? item->GetStackByID(a_stackID) : nullptr;
			if (!stack || !stack->extra || !stack->extra->IsItemBroken()) {
				return result;
			}

			// A broken item that is off stays off, locked or not. One that is
			// on, or being put back, is judged as if it were not broken, so a
			// lock still holds. kItemBroken hides the player's kNoEquipKeyword
			// test that comes after it, playerCannotEquip on super mutant
			// armor, so a broken piece with that keyword can come off the
			// player, or go back on, where an unbroken one could not. Only the
			// console or another mod can put the player in one.
			const bool on = stack->IsEquipped();
			const bool back = t_puttingBack == a_handleID;
			auto answer = result;
			if (!on && !back) {
				answer = Result::kItemBroken;
			} else if (result == Result::kItemBroken) {
				answer = Result::kSuccess;
			}

			// CanEquip checked the object before it answered any of the 3.
			if (answer != result) {
				const auto* why = on ? "but may come off" : (back ? "but goes back on" : "so it stays off");
				TraceLog::For(a_actor != RE::PlayerCharacter::GetSingleton())
					.Line("equip", "{:s} [{:08X}] is broken, {:s}", TraceLog::Who{ item->object }, item->object->formID, why);
			}
			return answer;
		}

		// Marks the item being put back for CanEquipHk while the toggle runs.
		bool PutBackHk(RE::ActorEquipManager* a_manager, RE::Actor* a_actor, const std::uint32_t& a_handleID, std::uint32_t a_stackID,
			const RE::BGSEquipSlot* a_slot, bool a_allowUnequip, bool a_locked)
		{
			const auto outer = std::exchange(t_puttingBack, a_handleID);
			const auto done = a_manager->ToggleEquipInventoryItem(a_actor, a_handleID, a_stackID, a_slot, a_allowUnequip, a_locked);
			t_puttingBack = outer;
			return done;
		}
	}

	void Install()
	{
		// Both or neither: the button without the toggle offers what the
		// toggle refuses.
		const auto canEquip = RE::ID::ActorEquipManager::CanEquip.address();
		const auto check = reinterpret_cast<std::uintptr_t>(&CanEquipHk);
		if (!CallPatch::PatchTogether({ { TOGGLE_SITE, canEquip, check }, { BUTTON_SITE, canEquip, check } })) {
			REX::ERROR("A broken item that is on will be stuck on.");
			return;
		}
		REX::INFO("A broken item comes off when asked and stays off until it is repaired.");

		// The put back hook only marks what CanEquipHk reads, so it goes in
		// after it or not at all.
		const auto toggle = RE::ID::ActorEquipManager::ToggleEquipInventoryItem.address();
		if (!CallPatch::PatchCall(PUT_BACK_SITE, toggle, reinterpret_cast<std::uintptr_t>(&PutBackHk))) {
			REX::ERROR("A broken piece taken off for the barber chair or the surgeon stays off.");
		}
	}
}
