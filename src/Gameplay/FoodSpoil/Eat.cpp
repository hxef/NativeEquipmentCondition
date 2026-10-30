#include "Gameplay/FoodSpoil/Food.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/TraceLog.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "Gameplay/FoodSpoil/Kinds.h"
#include "Gameplay/FoodSpoil/Stamp.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace FoodSpoil
{
	namespace
	{
		// -------------------------------------------------------------------
		// The bite
		// -------------------------------------------------------------------

		// What eating hands the cast, the same on both game versions. The
		// game queues it and runs it later, often in another frame.
		struct DrinkTask
		{
			RE::MagicItem*     food;      // 00
			RE::TESObjectREFR* eater;     // 08
			void*              extra;     // 10
			std::uintptr_t     callback;  // 18, the one that removes the piece eaten
			std::uint32_t      stackID;   // 20, the stack it is taken from
		};

		// The 1 call that casts the food eaten and then removes the piece.
		constexpr CallPatch::CallSite BITE_SITE{ CallPatch::AE{ RE::ID::TaskQueueInterface::TaskUnpackFunc.ae(), 0x167F }, CallPatch::OG{ RE::ID::TaskQueueInterface::TaskUnpackFunc.og(), 0x16FA }, "bite mark" };

		// What a spoiled bite gives of the food's own healing and bonuses.
		constexpr float SPOILED_SHARE = 0.1F;

		// The rads a spoiled bite adds on top of the food's own, as in
		// Fallout 76: meat and dishes made with raw food 15, crops and other
		// dishes 5.
		constexpr float MEAT_RADS = 15.0F;
		constexpr float PLANT_RADS = 5.0F;

		CallPatch::Held                                                                  g_bite;
		CallPatch::Link<void (*)(RE::MagicCaster*, const DrinkTask*)>                    g_biteLink;
		CallPatch::Link<void (*)(RE::ActorMagicCaster*, RE::ActiveEffect*, float, bool)> g_adjustLink;
		REL::Relocation<void (*)(RE::ActorMagicCaster*, RE::ActiveEffect*, float, bool)> _AdjustActiveEffect;

		// The food of a spoiled bite while its cast runs, on the thread that
		// casts it. The cast makes every effect inside the call.
		thread_local const RE::MagicItem* t_spoiledBite = nullptr;

		// How many pieces a_item's stacks hold together.
		std::uint32_t Pieces(const RE::BGSInventoryItem& a_item)
		{
			std::uint32_t pieces = 0;
			for (const auto* stack = a_item.stackData.get(); stack; stack = stack->nextStack.get()) {
				pieces += stack->count;
			}
			return pieces;
		}

		// How many pieces of a_food the player holds, or nothing when its
		// stack a_stackID is neither spoiled nor out of time.
		std::optional<std::uint32_t> SpoiledHeld(RE::TESObjectREFR& a_player, const RE::TESBoundObject& a_food, std::uint32_t a_stackID)
		{
			auto* inv = a_player.inventoryList;
			if (!inv) {
				return std::nullopt;
			}
			const auto               today = Today();
			const RE::BSAutoReadLock l(inv->rwLock);
			for (const auto& item : inv->data) {
				if (item.object != &a_food) {
					continue;
				}
				const auto* stack = item.GetStackByID(a_stackID);
				auto*       extra = stack ? stack->extra.get() : nullptr;
				const bool  spoiled = today ? FreshnessIn(KindOf(a_food), extra, false, *today) <= 0.0F : IsSpoiled(extra);
				if (!spoiled) {
					return std::nullopt;
				}
				return Pieces(item);
			}
			return std::nullopt;
		}

		std::uint32_t PiecesHeld(RE::TESObjectREFR& a_player, const RE::TESBoundObject& a_food)
		{
			auto* inv = a_player.inventoryList;
			if (!inv) {
				return 0;
			}
			std::uint32_t            pieces = 0;
			const RE::BSAutoReadLock l(inv->rwLock);
			for (const auto& item : inv->data) {
				if (item.object == &a_food) {
					pieces += Pieces(item);
				}
			}
			return pieces;
		}

		// Whether a_dish is made from raw food by any recipe.
		bool MadeWithRaw(const RE::TESBoundObject& a_dish)
		{
			for (const auto* recipe : g_dataHandler->GetFormArray<RE::BGSConstructibleObject>()) {
				if (!recipe || recipe->createdItem != &a_dish || !recipe->requiredItems) {
					continue;
				}
				for (const auto& part : *recipe->requiredItems) {
					if (part.first && KindOf(*part.first) == Kind::kRaw) {
						return true;
					}
				}
			}
			return false;
		}

		// The rads a spoiled bite of a_food adds.
		float RadsOf(const RE::TESBoundObject& a_food)
		{
			switch (KindOf(a_food)) {
			case Kind::kRaw:
				return MEAT_RADS;
			case Kind::kCooked:
				return MadeWithRaw(a_food) ? MEAT_RADS : PLANT_RADS;
			default:
				return PLANT_RADS;
			}
		}

		// Rads, found by editor ID. The game keeps an actor value's editor ID.
		const RE::ActorValueInfo* Rads()
		{
			for (const auto* value : g_dataHandler->GetFormArray<RE::ActorValueInfo>()) {
				if (value && std::string_view{ value->GetFormEditorIDString().c_str() } == "Rads") {
					return value;
				}
			}
			return nullptr;
		}

		// Adds a_rads as the food's own rad effect does: as damage, which on
		// Rads counts up.
		void AddRads(RE::Actor& a_player, float a_rads)
		{
			if (const auto* rads = Rads()) {
				a_player.ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, *rads, a_rads);
			}
		}

		// The cast of food anybody eats. Marks a spoiled bite of the player's
		// for the effects the cast makes, then adds its rads once the piece is
		// gone. Writes nothing to the stack: the sweep and the door turn it
		// spoiled later.
		void BiteHk(RE::MagicCaster* a_caster, const DrinkTask* a_task)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!g_bite.Runs(g_biteLink) || !a_task || !a_task->food || !player || a_task->eater != player ||
				a_task->stackID == std::numeric_limits<std::uint32_t>::max() ||
				a_task->callback != RE::ID::Actor::DrinkPotionCallback.address() || !Spoils(*a_task->food)) {
				g_biteLink(a_caster, a_task);
				return;
			}
			const auto& food = *a_task->food;
			const auto  before = SpoiledHeld(*player, food, a_task->stackID);
			if (!before) {
				g_biteLink(a_caster, a_task);
				return;
			}

			t_spoiledBite = &food;
			g_biteLink(a_caster, a_task);
			t_spoiledBite = nullptr;

			// A cast that fails removes nothing, and then nothing was eaten.
			if (PiecesHeld(*player, food) >= *before) {
				return;
			}
			const auto rads = RadsOf(food);
			AddRads(*player, rads);
			TraceLog::Line("food", "ate spoiled {:s} [{:08X}] from stack {:d}: a tenth of its healing and +{:.0f} rads",
				RE::TESFullName::GetFullName(food), food.formID, a_task->stackID, rads);
		}

		// Each effect a cast makes, for every actor. Under the mark, an effect
		// of the spoiled food that is not harmful keeps a tenth of its
		// strength. The food's own rads are harmful, so they stay in full.
		void AdjustHk(RE::ActorMagicCaster* a_this, RE::ActiveEffect* a_effect, float a_power, bool a_hostileOnly)
		{
			_AdjustActiveEffect(a_this, a_effect, a_power, a_hostileOnly);
			if (!g_bite.Runs(g_adjustLink) || !t_spoiledBite || !a_effect || a_effect->spell != t_spoiledBite) {
				return;
			}
			const auto* effect = a_effect->effect;
			const auto* setting = effect ? effect->effectSetting : nullptr;
			if (setting && !setting->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kDetrimental)) {
				a_effect->magnitude *= SPOILED_SHARE;
			}
		}
	}

	void InstallEat(Part a_group)
	{
		// Both or neither: the tenth needs the mark, and a mark with no tenth
		// would add the rads to a bite that heals in full.
		const CallPatch::Together bite{ a_group };
		CallPatch::PatchCall(BITE_SITE, RE::ID::MagicCaster::CastWithCallback, reinterpret_cast<std::uintptr_t>(&BiteHk), g_biteLink);
		_AdjustActiveEffect = CallPatch::PatchSlot(RE::ActorMagicCaster::VTABLE[0], { 0x1D, 0x1D }, AdjustHk, "spoiled bite effects",
			Part::kNone, true, &g_adjustLink)
		                          .value_or(0);
		g_bite = bite.Set();
	}
}
