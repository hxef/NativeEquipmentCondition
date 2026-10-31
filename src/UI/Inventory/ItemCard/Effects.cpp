#include "UI/Inventory/ItemCard/Cards.h"

#include "Core/CallPatch/CallPatch.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"

#include <array>
#include <cstddef>
#include <optional>

// The numbers of spoiled food's effects, wherever the Pip-Boy or a menu shows
// them: a card's HP, bonus and Rads rows, the green health still to come on
// the Pip-Boy's HP bar, and the Pip-Boy's STAT Effects list after eating.
// The game works each number out in nsStatsMenuUtils::GetEffectDisplayInfo
// from the food's own effects, so NEC's hooks on the calls to it change only
// its answer, by what eating the food gives: SpoiledShare of each effect that
// is not harmful, and the rads it adds on the food's own Rads row. A food
// with no Rads row of its own gets a row of NEC's, see RadsRow.
//
// A card says which food it shows through t_foodCard. Everywhere else the
// effects are the ones running on the player, which read the share the
// player's last meal of that food kept, see FoodSpoil::EatenShare.
namespace ItemCard
{
	namespace
	{
		using CallPatch::AE;
		using CallPatch::CallSite;
		using CallPatch::OG;

		constexpr CallSite EFFECT_SITES[] = {
			// The row of 1 effect on the card of every menu but the Pip-Boy,
			// and on the Pip-Boy's card.
			{ AE{ 2222677, 0x0D1 }, OG{ 777326, 0x0D1 }, "food card effects" },
			{ AE{ 2225346, 0x0B4 }, OG{ 59112, 0x0B4 }, "food pipboy effects" },
			// 1 effect's share of the green health still to come on the
			// Pip-Boy's HP bar, for the card's food. On OG the effects running
			// on the player go through it too.
			{ AE{ 2249725, 0x047 }, OG{ 1406687, 0x047 }, "food health bar" },
			// STAT Effects, as it lists a running effect and as it updates one.
			{ AE{ 2225855, 0x212 }, OG{ 719507, 0x2B9 }, "food stat effects" },
			{ AE{ 2225857, 0x14C }, OG{ 675101, 0x15A }, "food stat effect" },
			// The green health still to come from the effects running on the
			// player, which OG works out through the health bar call above.
			{ AE{ 2249733, 0x0F9 }, CallPatch::NO_OG_COVERED, "food health to come" },
		};

		using Display_t = void (*)(RE::MagicItem*, RE::EffectItem*, float*, float*);

		std::array<CallPatch::Link<Display_t>, std::size(EFFECT_SITES)> g_links;

		// Whether a_setting is harmful, as eating counts it, see
		// FoodSpoil/Eat.cpp.
		bool Harmful(const RE::EffectSetting& a_setting)
		{
			return a_setting.data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kDetrimental);
		}

		// Adds a_card's rads to the first Rads row of the food's own. A row
		// whose magnitude counts per second shows it times a_duration.
		void AddRads(FoodCard& a_card, const RE::EffectSetting& a_setting, float& a_magnitude, float a_duration)
		{
			if (a_card.radsShown || a_setting.data.primaryAV != FoodSpoil::RadsValue()) {
				return;
			}
			a_card.radsShown = true;
			const bool perSecond = a_setting.data.archetype == RE::EffectArchetypes::ArchetypeID::kValueModifier && a_duration > 0.0F &&
			                       !a_setting.data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kRecover);
			a_magnitude += perSecond ? a_card.rads / a_duration : a_card.rads;
		}

		// What a_effect of a_item gives the player, in a_magnitude, which
		// holds the vanilla answer.
		void Adjust(const RE::MagicItem& a_item, const RE::EffectSetting& a_setting, float& a_magnitude, float a_duration)
		{
			if (auto* card = t_foodCard) {
				if (card->food != &a_item) {
					return;
				}
				if (Harmful(a_setting)) {
					AddRads(*card, a_setting, a_magnitude, a_duration);
				} else {
					a_magnitude *= card->share;
				}
				return;
			}
			if (!Harmful(a_setting)) {
				if (const auto share = FoodSpoil::EatenShare(a_item)) {
					a_magnitude *= *share;
				}
			}
		}

		template <std::size_t I>
		void DisplayHk(RE::MagicItem* a_item, RE::EffectItem* a_effect, float* a_magnitude, float* a_duration)
		{
			g_links[I](a_item, a_effect, a_magnitude, a_duration);
			if (g_links[I].Live() && a_item && a_effect && a_effect->effectSetting && a_magnitude && a_duration) {
				Adjust(*a_item, *a_effect->effectSetting, *a_magnitude, *a_duration);
			}
		}
	}

	FoodCard FoodCardOf(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack)
	{
		const auto* object = a_item.object;
		if (!object || !object->IsAlchemyItem() || !FoodSpoil::EatingRuns() ||
			!FoodSpoil::SpoiledStack(*object, a_stack ? a_stack->extra.get() : nullptr)) {
			return {};
		}
		const auto& food = static_cast<const RE::AlchemyItem&>(*object);
		return { &food, FoodSpoil::SpoiledShare(), FoodSpoil::ExtraRads(food) };
	}

	std::optional<float> RadsRow(const FoodCard& a_card)
	{
		return a_card.food && !a_card.radsShown && a_card.rads > 0.0F ? std::optional{ a_card.rads } : std::nullopt;
	}

	const char* RadsText()
	{
		const auto* rads = FoodSpoil::RadsValue();
		if (!rads) {
			return "";
		}
		const auto* text = rads->GetAbbreviation();
		return text && *text ? text : RE::TESFullName::GetFullName(*rads).data();
	}

	void PatchFoodEffects()
	{
		const auto hooks = CallPatch::PerSite<std::size(EFFECT_SITES)>([]<std::size_t I>() { return &DisplayHk<I>; });
		CallPatch::PatchAll(EFFECT_SITES, RE::ID::nsStatsMenuUtils::GetEffectDisplayInfo, hooks, g_links,
			"Menus show what spoiled food gives", Part::kFoodBite);
	}
}
