#include "Gameplay/FoodSpoil/Food.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "Gameplay/FoodSpoil/Kinds.h"
#include "Gameplay/FoodSpoil/Stamp.h"

#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

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

		CallPatch::Held                                                                  g_bite;
		CallPatch::Link<void (*)(RE::MagicCaster*, const DrinkTask*)>                    g_biteLink;
		CallPatch::Link<void (*)(RE::ActorMagicCaster*, RE::ActiveEffect*, float, bool)> g_adjustLink;
		REL::Relocation<void (*)(RE::ActorMagicCaster*, RE::ActiveEffect*, float, bool)> _AdjustActiveEffect;

		// The spoiled food being eaten while its cast runs, and the share of
		// its strength each effect keeps, on the thread that casts it. The
		// cast makes every effect inside the call.
		thread_local const RE::MagicItem* t_spoiledBite = nullptr;
		thread_local float                t_share = 0.0F;

		// The food the player last ate spoiled, each with the share its
		// effects kept, for the menus that show them, see EatenShare.
		std::mutex                                             g_eatenLock;
		std::vector<std::pair<const RE::MagicItem*, float>> g_eaten;

		void NoteEaten(const RE::MagicItem& a_food, float a_share)
		{
			const std::scoped_lock l{ g_eatenLock };
			std::erase_if(g_eaten, [&](const auto& a_eaten) { return a_eaten.first == &a_food; });
			g_eaten.emplace_back(&a_food, a_share);
		}

		void ForgetEaten(const RE::MagicItem& a_food)
		{
			const std::scoped_lock l{ g_eatenLock };
			std::erase_if(g_eaten, [&](const auto& a_eaten) { return a_eaten.first == &a_food; });
		}

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

		// Adds a_rads as the food's own rad effect does: as damage, which on
		// Rads counts up.
		void AddRads(RE::Actor& a_player, float a_rads)
		{
			if (const auto* rads = RadsValue(); rads && a_rads > 0.0F) {
				a_player.ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, *rads, a_rads);
			}
		}

		// The cast of food anybody eats. Marks spoiled food the player eats
		// for the effects the cast makes, then adds its rads once the piece is
		// gone. Writes nothing to the stack: the sweep and the door turn it
		// spoiled later.
		void BiteHk(RE::MagicCaster* a_caster, const DrinkTask* a_task)
		{
			auto*      player = RE::PlayerCharacter::GetSingleton();
			const bool playerEats = g_bite.Runs(g_biteLink) && a_task && a_task->food && player && a_task->eater == player &&
			                        a_task->callback == RE::ID::Actor::DrinkPotionCallback.address();
			const auto before = playerEats && a_task->stackID != std::numeric_limits<std::uint32_t>::max() && Spoils(*a_task->food) ?
			                        SpoiledHeld(*player, *a_task->food, a_task->stackID) :
			                        std::nullopt;
			if (!before) {
				// Fresh food's effects take the place of the spoiled ones.
				if (playerEats) {
					ForgetEaten(*a_task->food);
				}
				g_biteLink(a_caster, a_task);
				return;
			}
			const auto& food = *a_task->food;

			// Read before the cast, as the food's own rads are.
			const auto share = SpoiledShare();
			const auto rads = ExtraRads(food);

			t_spoiledBite = &food;
			t_share = share;
			g_biteLink(a_caster, a_task);
			t_spoiledBite = nullptr;

			// A cast that fails removes nothing, and then nothing was eaten.
			if (PiecesHeld(*player, food) >= *before) {
				return;
			}
			AddRads(*player, rads);
			NoteEaten(food, share);
			const auto noted = InSurvival() && NoteSpoiledMeal(food);
			TraceLog::Line("food", "ate spoiled {:s} [{:08X}] from stack {:d}: {:.0f}% of its healing and +{:.0f} rads{:s}",
				RE::TESFullName::GetFullName(food), food.formID, a_task->stackID, share * 100.0F, rads,
				noted ? ", noted for Survival's hunger" : "");
		}

		// Each effect a cast makes, for every actor. Under the mark, an effect
		// of the spoiled food that is not harmful keeps t_share of its
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
				a_effect->magnitude *= t_share;
			}
		}
	}

	bool InSurvival()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		return player && player->GetDifficultyLevel() == RE::DifficultyLevel::kTrueSurvival;
	}

	float SpoiledShare()
	{
		return InSurvival() ? 0.0F : ShareOf(Settings::iSpoiledHealing.GetValue());
	}

	bool EatingRuns()
	{
		return g_bite.Intact();
	}

	std::optional<float> EatenShare(const RE::MagicItem& a_food)
	{
		if (!EatingRuns()) {
			return std::nullopt;
		}
		const std::scoped_lock l{ g_eatenLock };
		for (const auto& [food, share] : g_eaten) {
			if (food == &a_food) {
				return share;
			}
		}
		return std::nullopt;
	}

	void ForgetEaten()
	{
		const std::scoped_lock l{ g_eatenLock };
		g_eaten.clear();
	}

	void InstallEat(Part a_group)
	{
		// Both or neither: the share needs the mark, and a mark with no share
		// would add the rads to food that heals in full.
		const CallPatch::Together bite{ a_group };
		CallPatch::PatchCall(BITE_SITE, RE::ID::MagicCaster::CastWithCallback, reinterpret_cast<std::uintptr_t>(&BiteHk), g_biteLink);
		_AdjustActiveEffect = CallPatch::PatchSlot(RE::ActorMagicCaster::VTABLE[0], { 0x1D, 0x1D }, AdjustHk, "spoiled bite effects",
			Part::kNone, true, &g_adjustLink)
		                          .value_or(0);
		g_bite = bite.Set();
	}
}
