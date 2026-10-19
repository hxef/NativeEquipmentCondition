#include "Condition/WeaponWear/WeaponWear.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Condition/WeaponWear/Copy.h"
#include "Condition/WeaponWear/Rate.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <cstdint>
#include <string_view>

namespace WeaponWear
{
	namespace
	{
		// What the game says when a weapon breaks. sWeaponBreak is a game
		// setting Fallout 4 still has but never reads, the message the older
		// games printed. Its text is read at each break, so a change during
		// play shows. The setting is built into the game, so the pointer
		// outlives a full reset. Null when it is missing, which only loses the
		// message.
		const RE::Setting* g_breakMessage = nullptr;

		std::string_view BreakMessage()
		{
			return g_breakMessage ? g_breakMessage->GetString() : ""sv;
		}
	}

	void Load()
	{
		if (!g_dataHandler) {
			return;
		}

		MeasureReference(false);

		// Whatever this load order says, see g_breakMessage.
		auto* settings = RE::GameSettingCollection::GetSingleton();
		g_breakMessage = settings ? settings->GetSetting("sWeaponBreak"sv) : nullptr;

		const auto message = BreakMessage();
		if (message.empty()) {
			REX::WARN("sWeaponBreak is missing, so your weapon is put away without a word once it wears out. An NPC's stays in their hands.");
		} else {
			REX::INFO("Your weapon is put away once it wears out, and the game says \"{:s}\". An NPC's stays in their hands.", message);
		}

		const auto theirs = Settings::fNpcWearRateMult.GetValue();
		if (TheirsWear()) {
			REX::INFO("NPC weapons wear at fNpcWearRateMult {:g}.", theirs);
		} else {
			REX::INFO("NPC weapons never wear, since fNpcWearRateMult is {:g}.", theirs);
		}

		// A line only for an automatic speed that changes something.
		const auto automatic = AutoMult();
		if (automatic <= 0.0F) {
			REX::INFO("Automatic weapons, yours and NPCs', never wear as they fire or cut, since fAutoWearRateMult is {:g}. A bash still wears them.",
				Settings::fAutoWearRateMult.GetValue());
		} else if (automatic != 1.0F) {
			REX::INFO("Automatic weapons, yours and NPCs', wear {:g} times as fast as they fire or cut, by fAutoWearRateMult. A bash wears as any other weapon's does.",
				automatic);
		}

		// A line only for a start below full, see Condition::Share.
		const auto start = Condition::Start(Condition::Kind::kWeapon);
		if (start <= 0.0F) {
			REX::INFO("A weapon, yours or an NPC's, performs as new until it breaks, since fWeaponPenaltyStart is {:g}, and a broken one at its floors. Its price still falls from full.",
				Settings::fWeaponPenaltyStart.GetValue());
		} else if (start < 1.0F) {
			REX::INFO("A weapon, yours or an NPC's, performs as new down to {:.2f} condition, by fWeaponPenaltyStart, and below it each penalty runs down to its floor at 0. Its price still falls from full.",
				start);
		}
	}

	void MeasureAgain()
	{
		if (g_dataHandler) {
			MeasureReference(true);
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
		RE::TBO_InstanceData* BaseInstanceData(const RE::TESObjectWEAP& a_weapon)
		{
			// weaponData holds the unmodified stats, and TESObjectWEAP::Data
			// derives from TBO_InstanceData, so it can be used in place of the
			// per stack copy. Writing here would change every copy in the game.
			return const_cast<RE::TESObjectWEAP::Data*>(&a_weapon.weaponData);
		}

		// Picks the equipped stack and works out there what this use takes
		// off it, for Equipped::WearStack. Whether the copy is the keeper's own
		// is asked here as well, see Equipped::Issued, since the copy need not
		// carry every entry Issued reads.
		class FindStack final :
			public RE::BGSInventoryItem::StackDataCompareFunctor
		{
		public:
			FindStack(const RE::TESObjectWEAP& a_weapon, const char* a_source, float a_scale, bool a_theirs,
				const RE::Actor* a_keeper) noexcept :
				weapon(&a_weapon),
				keeper(a_keeper),
				wear(a_source, a_scale, a_theirs)
			{}

			bool CompareData(const RE::BGSInventoryItem::Stack& a_stack) override
			{
				if (!equipped.CompareData(a_stack)) {
					return false;
				}

				// The engine only builds per stack instance data for an item
				// with a mod list, so a plain weapon has none and the base
				// form's stats are used. The extra data entry can also exist
				// with an empty pointer, so the fallback checks the pointer.
				auto* xInstanceData = a_stack.extra->GetByType<RE::ExtraInstanceData>();
				auto* instanceData = xInstanceData ? xInstanceData->data.get() : nullptr;
				if (!instanceData) {
					instanceData = BaseInstanceData(*weapon);
				}

				// An essential NPC's own weapon never wears, the rule
				// ArmorEvents.h gives their own armor.
				if (keeper && Equipped::Issued(*keeper, *weapon, a_stack.extra.get())) {
					TraceLog::Npc::Line("wear", "{:s} [{:08X}] is their own and they are essential, so no wear",
						RE::TESFullName::GetFullName(*weapon), weapon->formID);
					return false;
				}

				// Listed once per weapon in the player's log, so an NPC's copy
				// never takes the player's listing.
				if (!wear.theirs) {
					LogDamageSources(*weapon, instanceData, xInstanceData && xInstanceData->data);
				}

				// What this weapon costs per use with the stats of the copy in
				// hand, mods included, times how many ordinary uses this event
				// counts as, see a_scale in WeaponWear.h.
				const auto& instance = *static_cast<const RE::TESObjectWEAP::InstanceData*>(instanceData);
				const bool  bash = std::string_view{ wear.source } == BASH;
				wear.amount = Rate(*weapon, instance, a_stack.extra.get(), wear.theirs, bash) * wear.scale;
				return Condition::Lowers(Condition::HealthOf(&a_stack), *weapon, wear.amount, wear.source, wear.theirs);
			}

			Equipped::EquippedStack  equipped;
			const RE::TESObjectWEAP* weapon;
			const RE::Actor*         keeper;
			Equipped::WearStack      wear;
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

				// The message is for the player, whose weapon alone comes
				// here, see Wear. The setting's text ends in a 0 byte, so
				// data() is safe to pass on.
				const auto message = BreakMessage();
				if (holder == RE::PlayerCharacter::GetSingleton() && !message.empty()) {
					RE::SendHUDMessage::ShowHUDMessage(message.data(), nullptr, true, false);
				}

				TraceLog::Line("wear", "{:s} [{:08X}] is worn out and has been put away",
					RE::TESFullName::GetFullName(*weapon), weapon->formID);
			});
		}
	}

	Worn WearCopy(RE::TESObjectREFR& a_owner, RE::TESObjectWEAP& a_weapon, const char* a_source, float a_scale, bool a_theirs,
		const RE::Actor* a_keeper)
	{
		FindStack find{ a_weapon, a_source, a_scale, a_theirs, a_keeper };
		auto&     wear = find.wear;
		Equipped::WriteStack(a_owner, a_weapon, find, wear);
		const auto count = find.equipped.count;

		// A split that leaves the health unchanged merges straight back, so
		// only a change is worth reporting.
		if (wear.moved && count > 1) {
			TraceLog::For(a_theirs).Line("wear", "worn copy split off a stack of {:d}", count);
		}
		return { count, wear.moved, wear.broke };
	}

	bool Wear(RE::TESObjectREFR& a_owner, RE::TESObjectWEAP& a_weapon, const char* a_source, float a_scale)
	{
		const auto worn = WearCopy(a_owner, a_weapon, a_source, a_scale, false, nullptr);

		// The inventory lock is released by here, which is what makes this
		// safe.
		if (worn.broke) {
			PutAway(a_owner, a_weapon);
		}
		return worn.moved;
	}
}
