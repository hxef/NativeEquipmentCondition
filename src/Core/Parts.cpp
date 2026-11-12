#include "Core/Parts.h"

#include "Core/Pieces.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <span>

// What each setting works through. The MCM page reads it to say which
// settings do nothing, or which of their pieces are off, while another mod
// has a part, and the Settings line marks them the same way, so no setting
// needs code of its own for it.
//
// config.json lays out its blocks by under. A number moved here moves there
// too.
namespace
{
	using enum Piece;

	// A shot's jam comes through Gun wear from firing's fire call, see
	// Core/Pieces.cpp. A single shot gun's comes through its own reload call.
	constexpr Piece JAM[]{ kJamShot };
	constexpr Piece JAM_SIDE[]{ kJamReload };
	// The 2 side parts roll loot lying in the world and a stock's items with
	// no mod slots. A lost set costs only what that set did, see its WARN
	// line. The 2 exceptions are cases Worn loot handles on their own, so
	// losing one takes nothing from the switch.
	constexpr Piece WORN_LOOT[]{ kWornLoot };
	constexpr Piece WORN_LOOT_SIDE[]{ kWorldLoot, kModlessLoot };
	constexpr Piece WORN_LOOT_EXCEPTIONS[]{ kConsoleNew, kGifts };
	// A repair is priced from a worn item's price, which only Worn item
	// prices gives. Without it a quote still scales, but from the item's
	// barter price.
	constexpr Piece TRADER_REPAIRS[]{ kTraderRepairs };
	constexpr Piece TRADER_REPAIRS_SIDE[]{ kStockMerchant, kStockLinked, kPrices };
	constexpr Piece TRADER_PRICE_SIDE[]{ kPrices };
	// How far a trader repairs comes from what their chests restock with.
	constexpr Piece TRADER_STOCK_SIDE[]{ kStockMerchant, kStockLinked };
	// A tip about a part that is off hides on purpose, so it loses nothing
	// else.
	constexpr Piece TIPS[]{ kTips };
	constexpr Piece HUD_BARS[]{ kHudBars };
	constexpr Piece PICKUP_BAR[]{ kPickupBar };
	constexpr Piece LOOT_METERS[]{ kLootMeters };
	constexpr Piece CONFIRM_SCROLL[]{ kConfirmScroll };
	constexpr Piece INSPECT_PRICE[]{ kInspectPrice };
	// Melee hits and bashes wear a weapon through a hit event no mod can take,
	// so it never does nothing.
	constexpr Piece GUN_WEAR[]{ kGunWear };
	constexpr Piece DAMAGE[]{ kDmgMeleePhysical, kDmgMeleeEnergy, kDmgGunPhysical, kDmgGunEnergy, kDmgBlastPhysical,
		kDmgBlastEnergy, kDmgBlastObjects, kDmgHitEffects, kDmgBlastEffects, kCardDmgPhysical, kCardDmgEnergy,
		kCardDmgModEffects, kCardDmgOwnEffects, kCardDmgBlast };
	constexpr Piece ARMOR[]{ kArmorDr, kArmorEnergyRad, kArmorBestMark, kArmorNpcRanking, kCardArmorMenus, kCardArmorCompare,
		kCardArmorPipboy, kCardArmorDoll };
	// Read only inside Worn item prices' own hook.
	constexpr Piece PRICES[]{ kPrices };
	constexpr Piece FIRE_RATE[]{ kFireRate };
	constexpr Piece FIRE_RATE_SIDE[]{ kFireRateNpcGuns, kFireSound, kRateCardsContainers, kRateCardsWorkbench, kRateCardsCooking,
		kRateCardsPipboy, kRateBetter, kRateSort };
	// Your meter and an NPC's roll each work alone.
	constexpr Piece CRIT_METER[]{ kCritMeter, kNpcCrits };
	// Every shot's spread, yours and every NPC's, and the VATS hit chance
	// each work alone. The crosshair and the menus show the spread.
	constexpr Piece ACCURACY[]{ kAccuracy, kVatsAccuracy };
	constexpr Piece ACCURACY_SIDE[]{ kCrosshair, kAccuracyCardsContainers, kAccuracyCardsWorkbench, kAccuracyCardsCooking,
		kAccuracyCardsPipboy, kAccuracySort };
	constexpr Piece RANGE[]{ kRange };
	constexpr Piece RANGE_SIDE[]{ kRangeCardsContainers, kRangeCardsWorkbench, kRangeCardsCooking, kRangeCardsPipboy };

	// Every piece of Lists, in their order.
	template <const auto&... Lists>
	consteval auto Joined()
	{
		std::array<Piece, (std::size(Lists) + ...)> joined{};
		std::size_t                                 i = 0;
		for (const auto list : { std::span<const Piece>{ Lists }... }) {
			for (const auto piece : list) {
				joined[i++] = piece;
			}
		}
		return joined;
	}

	// The weapon start moves every weapon penalty's line, so it does
	// nothing only once all of them are off. The jams have a start of their
	// own.
	constexpr auto WEAPON_PENALTIES = Joined<DAMAGE, FIRE_RATE, CRIT_METER, ACCURACY, RANGE>();
	constexpr auto WEAPON_PENALTIES_SIDE = Joined<FIRE_RATE_SIDE, ACCURACY_SIDE, RANGE_SIDE>();

	constexpr Piece BENCH[]{ kBench };
	// The repair boxes' rows are laid at the bench and at a trader alike.
	constexpr Piece REPAIR_BOXES[]{ kBench, kTraderRepairs };
	// The perk pages tell of a discount on a bench repair, and the bench's
	// lists show what it repairs, so the line of each part shows in this
	// block.
	constexpr Piece BENCH_EXCEPTIONS[]{ kPerksChart, kPerksPipboy, kBenchItemList, kBenchModLists };
	// With another mod on the bench lists the lock still refuses BUILD, but
	// no row fades and the mod slots stay open.
	constexpr Piece MOD_LOCK_SIDE[]{ kBenchItemList, kBenchModLists };
	// Spare repair rides on the bench's REPAIR press, and the perk pages
	// tell of what Scrapper adds to a spare.
	constexpr Piece SPARE_SIDE[]{ kPerksChart, kPerksPipboy };
	// The bite, the station rows and the Fertilizer row each go off alone,
	// and food still spoils. The 2 days and the fridge speed set only how fast
	// it spoils.
	constexpr Piece FOOD[]{ kFoodSpoils };
	// What 1 Fertilizer costs counts only while the Fertilizer row is in.
	constexpr Piece FERTILIZER[]{ kFertilizer };
	constexpr Piece FOOD_SIDE[]{ kFoodBite, kFoodHunger, kFoodCook, kFertilizer };
	// What spoiled food gives comes through eating and the menus that show
	// it, and in Survival its hunger and sickness through Survival's read too.
	constexpr Piece FOOD_BITE[]{ kFoodBite };
	constexpr Piece FOOD_HUNGER[]{ kFoodHunger };
	// The fridge icon beside a fridge's name rides on the loot preview's
	// rows, and the one on a food's card on the container screen's CND row.
	constexpr Piece FRIDGE_ICON_SIDE[]{ kLootMeters, kCndContainers };

	constexpr SettingLink SETTINGS[]{
		{ &Settings::bSpawnCondition, nullptr, WORN_LOOT, WORN_LOOT_SIDE, WORN_LOOT_EXCEPTIONS },
		{ &Settings::bVendorRepair, nullptr, TRADER_REPAIRS, TRADER_REPAIRS_SIDE },
		{ &Settings::bTraderSign, &Settings::bVendorRepair, TRADER_REPAIRS },
		{ &Settings::bSpareRepair, &Settings::fBenchCostMult, BENCH, SPARE_SIDE },
		{ &Settings::iUnscrappableSpares, &Settings::fBenchCostMult, BENCH },
		{ &Settings::iRepairBoxRow, &Settings::fBenchCostMult, BENCH },
		{ &Settings::bRepairBoxColumns, &Settings::fBenchCostMult, REPAIR_BOXES },
		{ &Settings::bRepairBoxGains, &Settings::fBenchCostMult, REPAIR_BOXES },
		{ &Settings::bLoadingTips, nullptr, TIPS },
		{ &Settings::bHudCondition, nullptr, HUD_BARS },
		// The power armor bar's switch rides on HUD condition bars as the
		// rows below do, so it sits in bHudCondition's block.
		{ &Settings::bPowerArmorBar, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::bQuickContainer, nullptr, LOOT_METERS },
		{ &Settings::bConfirmScroll, nullptr, CONFIRM_SCROLL },
		{ &Settings::bInspectPrice, nullptr, INSPECT_PRICE },
		{ &Settings::bFoodSpoils, nullptr, FOOD, FOOD_SIDE },
		{ &Settings::bFridgeIcon, &Settings::bFoodSpoils, FOOD, FRIDGE_ICON_SIDE },
		{ &Settings::fWearRateMult, nullptr, {}, GUN_WEAR, {}, kMeleeWear },
		// An NPC's gun wears through the same fire call, so it sits in the
		// player's block and loses its guns with it.
		{ &Settings::fNpcWearRateMult, &Settings::fWearRateMult, {}, GUN_WEAR, {}, kMeleeWear },
		// An automatic's shots wear through the same fire call. A motor
		// blade's cuts wear through the hit event, so this row still sets
		// those.
		{ &Settings::fAutoWearRateMult, &Settings::fWearRateMult, {}, GUN_WEAR, {}, kMotorWear },
		{ &Settings::fDamageFloor, nullptr, DAMAGE },
		{ &Settings::fWeaponPenaltyStart, &Settings::fDamageFloor, WEAPON_PENALTIES, WEAPON_PENALTIES_SIDE },
		{ &Settings::fArmorFloor, nullptr, ARMOR },
		{ &Settings::fArmorPenaltyStart, &Settings::fArmorFloor, ARMOR },
		{ &Settings::iLootShift, &Settings::bSpawnCondition, WORN_LOOT },
		{ &Settings::iLootSurpriseOdds, &Settings::bSpawnCondition, WORN_LOOT },
		{ &Settings::fValueExponent, nullptr, PRICES },
		{ &Settings::fFireRateFloor, nullptr, FIRE_RATE, FIRE_RATE_SIDE },
		{ &Settings::fCritMeterFloor, nullptr, CRIT_METER },
		{ &Settings::fAccuracyFloor, nullptr, ACCURACY, ACCURACY_SIDE },
		{ &Settings::fRangeFloor, nullptr, RANGE, RANGE_SIDE },
		{ &Settings::fJamChance, nullptr, JAM, JAM_SIDE },
		{ &Settings::fJamStart, &Settings::fJamChance, JAM, JAM_SIDE },
		{ &Settings::fBenchCostMult, nullptr, BENCH, {}, BENCH_EXCEPTIONS },
		{ &Settings::fSpareWorthMult, &Settings::fBenchCostMult, BENCH },
		{ &Settings::iFreeMendAbove, &Settings::fBenchCostMult, BENCH },
		{ &Settings::fModLockStart, &Settings::fBenchCostMult, BENCH, MOD_LOCK_SIDE },
		{ &Settings::fTraderPriceMult, &Settings::bVendorRepair, TRADER_REPAIRS, TRADER_PRICE_SIDE },
		{ &Settings::iTraderLeeway, &Settings::bVendorRepair, TRADER_REPAIRS, TRADER_STOCK_SIDE },
		{ &Settings::iTraderFullStock, &Settings::bVendorRepair, TRADER_REPAIRS, TRADER_STOCK_SIDE },
		{ &Settings::fRawFoodDays, &Settings::bFoodSpoils, FOOD },
		{ &Settings::fCookedFoodDays, &Settings::bFoodSpoils, FOOD },
		{ &Settings::fFridgeSpoilSpeed, &Settings::bFoodSpoils, FOOD },
		{ &Settings::iSpoiledPerFertilizer, &Settings::bFoodSpoils, FERTILIZER },
		{ &Settings::iSpoiledHealing, &Settings::bFoodSpoils, FOOD_BITE },
		{ &Settings::iSpoiledHunger, &Settings::bFoodSpoils, FOOD_HUNGER, FOOD_BITE },
		{ &Settings::fSpoiledRadsMult, &Settings::bFoodSpoils, FOOD_BITE },
		{ &Settings::iSpoiledSicknessMult, &Settings::bFoodSpoils, FOOD_HUNGER, FOOD_BITE },
		// Both CND bars' rows share bHudCondition, so they never run once HUD
		// condition bars is left to another mod.
		{ &Settings::fHudBarX, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::fHudBarY, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::fHudBarLength, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::fHudBarThickness, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::bHudBarHidesWithCounter, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::bHudBarStandsAlone, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::bHudBarShowsWord, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::fPowerArmorBarX, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::fPowerArmorBarY, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::fPowerArmorBarLength, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::fPowerArmorBarThickness, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::bPowerArmorBarShowsWord, &Settings::bHudCondition, HUD_BARS },
		// The Pickup CND bar works through a place of its own and runs
		// whatever HUD condition bars does. Its rows sit in bHudCondition's
		// block, since MCM has no group left for a note of its own.
		{ &Settings::bPickupBar, &Settings::bHudCondition, PICKUP_BAR },
		{ &Settings::fPickupBarX, &Settings::bHudCondition, PICKUP_BAR },
		{ &Settings::fPickupBarY, &Settings::bHudCondition, PICKUP_BAR },
		{ &Settings::fPickupBarLength, &Settings::bHudCondition, PICKUP_BAR },
		{ &Settings::fPickupBarThickness, &Settings::bHudCondition, PICKUP_BAR },
		{ &Settings::bPickupBarShowsWord, &Settings::bHudCondition, PICKUP_BAR },
	};

	// Every setting changes something, no list names kNone or a piece twice,
	// and a piece that is always there is no other list's.
	consteval bool PiecesOnce()
	{
		for (const auto& link : SETTINGS) {
			if (link.core.empty() && link.always == kNone) {
				return false;
			}
			for (const auto list : { link.core, link.side, link.exceptions }) {
				for (const auto piece : list) {
					if (piece == kNone || Untakeable(piece)) {
						return false;
					}
					for (const auto other : { link.core, link.side, link.exceptions }) {
						if (std::ranges::count(other, piece) != (other.data() == list.data() ? 1 : 0)) {
							return false;
						}
					}
				}
			}
		}
		return true;
	}
	static_assert(PiecesOnce());

	// Every under is a setting that leads its own block.
	consteval bool UnderLeads()
	{
		for (const auto& link : SETTINGS) {
			if (!link.under) {
				continue;
			}
			bool found = false;
			for (const auto& lead : SETTINGS) {
				found = found || (!lead.under && lead.setting == link.under);
			}
			if (!found) {
				return false;
			}
		}
		return true;
	}
	static_assert(UnderLeads());
}

std::span<const SettingLink> SettingLinks()
{
	return SETTINGS;
}

const SettingLink* LinkOf(const Settings::Named& a_setting)
{
	const auto it = std::ranges::find(SETTINGS, &a_setting, &SettingLink::setting);
	return it != std::end(SETTINGS) ? &*it : nullptr;
}

std::bitset<PART_COUNT> PartsOf(const SettingLink& a_link)
{
	std::bitset<PART_COUNT> parts;
	for (const auto list : { a_link.core, a_link.side, a_link.exceptions }) {
		for (const auto piece : list) {
			parts.set(static_cast<std::size_t>(RowOfPiece(piece).part));
		}
	}
	parts.reset(static_cast<std::size_t>(Part::kNone));
	return parts;
}

std::bitset<PART_COUNT> CoreParts(const SettingLink& a_link)
{
	std::bitset<PART_COUNT> parts;
	for (const auto piece : a_link.core) {
		parts.set(static_cast<std::size_t>(RowOfPiece(piece).part));
	}
	return parts;
}
