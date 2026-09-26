#include "Condition/WeaponWear/WeaponWear.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Condition/WeaponWear/Rate.h"
#include "Core/TraceLog.h"

#include <cstdint>
#include <string>

namespace WeaponWear
{
	namespace
	{
		// What the game says when a weapon breaks. sWeaponBreak is a game
		// setting Fallout 4 still has but never reads, the message the older
		// games printed. Read as game data loads, and empty when the setting is
		// missing, which only loses the message.
		std::string g_breakMessage;
	}

	void Load()
	{
		if (!g_dataHandler) {
			return;
		}

		MeasureReference();

		// Whatever this load order says, see g_breakMessage.
		auto*       settings = RE::GameSettingCollection::GetSingleton();
		const auto* breaks = settings ? settings->GetSetting("sWeaponBreak"sv) : nullptr;
		if (breaks) {
			g_breakMessage = breaks->GetString();
		}

		if (g_breakMessage.empty()) {
			REX::WARN("sWeaponBreak is missing, so a weapon worn out will be put away without a word.");
		} else {
			REX::INFO("A weapon worn out is put away, and the game says \"{:s}\".", g_breakMessage);
		}
	}

	const char* WhyNoCondition(const RE::TESObjectWEAP& a_weapon)
	{
		// kNotPlayable marks the weapons the player is never meant to hold. A
		// null instance reads the flag off the record itself.
		if (!a_weapon.GetPlayable(nullptr)) {
			return "not playable";
		}

		if (a_weapon.IsThrownWeapon()) {
			return "thrown weapon";
		}

		return nullptr;
	}

	namespace
	{
		// The base form's own instance data, for items carrying none. Read
		// only.
		RE::TBO_InstanceData* BaseInstanceData(const RE::TESBoundObject& a_object)
		{
			// weaponData holds the unmodified stats, and TESObjectWEAP::Data
			// derives from TBO_InstanceData, so it can be used in place of the
			// per stack copy. Writing here would change every copy in the game.
			// IsWeapon reads the form type byte, so it costs nothing and never
			// comes back empty like a runtime cast can.
			if (!a_object.IsWeapon()) {
				return nullptr;
			}
			const auto& weapon = static_cast<const RE::TESObjectWEAP&>(a_object);
			return const_cast<RE::TESObjectWEAP::Data*>(&weapon.weaponData);
		}

		// Wears down the one stack the engine hands it, and remembers whether
		// the health changed. The engine has already split the stack if it held
		// more than one item, see Equipped::WriteEquipped, so the weapon in
		// hand and the weapon that wears stay the same one.
		class WearStack final :
			public RE::BGSInventoryItem::StackDataWriteFunctor
		{
		public:
			WearStack(const char* a_source, float a_scale) noexcept :
				source(a_source),
				scale(a_scale)
			{}

			void WriteDataImpl(RE::TESBoundObject& a_object, RE::BGSInventoryItem::Stack& a_stack) override
			{
				if (!a_stack.extra) {
					return;
				}

				// The engine only builds per stack instance data for an item
				// with a mod list, so a plain weapon has none and the base
				// form's stats are used. The extra data entry can also exist
				// with an empty pointer, so the fallback checks the pointer.
				auto* xInstanceData = a_stack.extra->GetByType<RE::ExtraInstanceData>();
				auto* instanceData = xInstanceData ? xInstanceData->data.get() : nullptr;
				if (!instanceData) {
					instanceData = BaseInstanceData(a_object);
				}

				// Every line names the item and the event, since guns and melee
				// come through different code on different threads.
				const auto name = RE::TESFullName::GetFullName(a_object);
				if (!instanceData) {
					REX::WARN("{:s}: {:s} [{:08X}] has no instance data and no base data, condition will not be decreased",
						source, name, a_object.formID);
					return;
				}

				LogDamageSources(a_object, instanceData, xInstanceData != nullptr);

				// The stats of the copy in hand, mods included. WearsOut has
				// already made sure this is a weapon.
				const auto& weapon = static_cast<const RE::TESObjectWEAP&>(a_object);
				const auto& instance = *static_cast<const RE::TESObjectWEAP::InstanceData*>(instanceData);

				// What this weapon costs per use, times how many ordinary uses
				// this event counts as, above 1 only for a bash or a power
				// attack.
				const auto amount = Rate(weapon, instance, a_stack.extra.get()) * scale;
				moved = Condition::Decrease(*a_stack.extra, a_object, amount, source, scale);

				// The engine's own broken check, run on the health just
				// written. It is the same check that stops the weapon being
				// equipped again, see MIN_HEALTH in Condition.h.
				broke = moved && a_stack.extra->IsItemBroken();
			}

			const char* source;
			float       scale;
			bool        moved{ false };
			bool        broke{ false };
		};

		// Takes a weapon just worn to 0 out of the owner's hands. The game
		// refuses to equip a broken weapon, see MIN_HEALTH in Condition.h, but
		// has no rule for one that breaks while held, so the shot that breaks a
		// weapon puts it away and the game's refusal keeps it away. Queued,
		// since this runs in the middle of a shot the engine has not finished
		// firing. F4SE runs the task a moment later from the game's own queue,
		// on a worker thread during play.
		void PutAway(RE::TESObjectREFR& a_owner, RE::TESObjectWEAP& a_weapon)
		{
			auto*       actor = a_owner.As<RE::Actor>();
			const auto* tasks = actor ? F4SE::GetTaskInterface() : nullptr;
			if (!tasks) {
				return;
			}

			// A handle, since the owner can die between this frame and the
			// next. Forms are only freed on a full reset, which waits for the
			// player to answer a prompt, so the weapon pointer is still valid.
			const auto handle = actor->GetHandle();
			auto*      weapon = &a_weapon;
			tasks->AddTask([handle, weapon]() {
				const auto owner = handle.get();
				auto*      holder = owner ? owner->As<RE::Actor>() : nullptr;
				auto*      equipment = RE::ActorEquipManager::GetSingleton();
				if (!holder || !equipment) {
					return;
				}

				// Null instance data lets the engine work out which copy is in
				// hand, as it does when it equips one. Equipped::ANY_STACK does
				// the same for the stack, and the rest is what the game passes
				// for an unequip from the Pip-Boy.
				const RE::BGSObjectInstance instance{ weapon, nullptr };
				equipment->UnequipObject(holder, &instance, 1, nullptr, Equipped::ANY_STACK,
					false, false, true, false, nullptr);

				// Only the player is told, since only the player sees the
				// message in the corner of the screen.
				if (holder == RE::PlayerCharacter::GetSingleton() && !g_breakMessage.empty()) {
					RE::SendHUDMessage::ShowHUDMessage(g_breakMessage.c_str(), nullptr, true, false);
				}

				TraceLog::Line("wear", "{:s} [{:08X}] is worn out and has been put away",
					RE::TESFullName::GetFullName(*weapon), weapon->formID);
			});
		}
	}

	bool Wear(RE::TESObjectREFR& a_owner, RE::TESObjectWEAP& a_weapon, const char* a_source, float a_scale)
	{
		WearStack  wear{ a_source, a_scale };
		const auto count = Equipped::WriteEquipped(a_owner, a_weapon, wear);

		// A split that leaves the health unchanged merges straight back, so
		// only a change is worth reporting.
		if (wear.moved && count > 1) {
			TraceLog::Line("wear", "worn copy split off a stack of {:d}", count);
		}

		// The inventory lock is released by here, which is what makes this
		// safe.
		if (wear.broke) {
			PutAway(a_owner, a_weapon);
		}
		return wear.moved;
	}
}
