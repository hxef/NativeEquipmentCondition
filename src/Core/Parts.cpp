#include "Core/Parts.h"

#include "Core/Pieces.h"

#include <algorithm>
#include <cstddef>
#include <iterator>

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

	// A shot's jam comes through Gun wear from firing, which Jam's row rides
	// on, see Features.cpp. A single shot gun's comes through its own reload
	// call.
	constexpr Piece JAM[]{ kJamShot };
	constexpr Piece JAM_SIDE[]{ kJamReload };
	constexpr Piece FIRE_RATE[]{ kFireRate };
	constexpr Piece FIRE_RATE_SIDE[]{ kFireRateNpcGuns, kFireSound, kRateCardsContainers, kRateCardsWorkbench, kRateCardsCooking,
		kRateCardsPipboy, kRateBetter, kRateSort };
	constexpr Piece CRIT_METER[]{ kCritMeter };
	constexpr Piece CRIT_METER_SIDE[]{ kNpcCrits };
	// The 3 exceptions are cases Worn loot handles on their own, so losing one
	// takes nothing from the switch.
	constexpr Piece WORN_LOOT[]{ kWornLoot };
	constexpr Piece WORN_LOOT_EXCEPTIONS[]{ kConsoleNew, kOldSaves, kGifts };
	// A repair is priced from a worn item's price, which only Worn item
	// prices gives. Without it a quote still scales, but from the game's own
	// barter price.
	constexpr Piece TRADER_REPAIRS[]{ kTraderRepairs };
	constexpr Piece TRADER_REPAIRS_SIDE[]{ kStockMerchant, kStockLinked, kPrices };
	constexpr Piece TRADER_PRICE_SIDE[]{ kPrices };
	// A tip about a part that is off hides on purpose, so it loses nothing
	// else.
	constexpr Piece TIPS[]{ kTips };
	constexpr Piece HUD_BARS[]{ kHudBars };
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
	constexpr Piece BENCH[]{ kBench };
	// The perk pages tell of a discount on a bench repair, and the bench's
	// lists show what it repairs, so the line of each part shows in this
	// block.
	constexpr Piece BENCH_EXCEPTIONS[]{ kPerksChart, kPerksPipboy, kBenchItemList, kBenchModLists };

	constexpr SettingLink SETTINGS[]{
		{ &Settings::bJam, nullptr, JAM, JAM_SIDE },
		{ &Settings::bFireRate, nullptr, FIRE_RATE, FIRE_RATE_SIDE },
		{ &Settings::bCritMeter, nullptr, CRIT_METER, CRIT_METER_SIDE },
		{ &Settings::bSpawnCondition, nullptr, WORN_LOOT, {}, WORN_LOOT_EXCEPTIONS },
		{ &Settings::bVendorRepair, nullptr, TRADER_REPAIRS, TRADER_REPAIRS_SIDE },
		{ &Settings::bLoadingTips, nullptr, TIPS },
		{ &Settings::bHudCondition, nullptr, HUD_BARS },
		{ &Settings::bQuickContainer, nullptr, LOOT_METERS },
		{ &Settings::bConfirmScroll, nullptr, CONFIRM_SCROLL },
		{ &Settings::bInspectPrice, nullptr, INSPECT_PRICE },
		{ &Settings::fWearRateMult, nullptr, {}, GUN_WEAR, {}, kMeleeWear },
		{ &Settings::fDamageFloor, nullptr, DAMAGE },
		{ &Settings::fArmorFloor, nullptr, ARMOR },
		{ &Settings::fValueExponent, nullptr, PRICES },
		{ &Settings::fFireRateFloor, &Settings::bFireRate, FIRE_RATE, FIRE_RATE_SIDE },
		{ &Settings::fCritMeterFloor, &Settings::bCritMeter, CRIT_METER, CRIT_METER_SIDE },
		{ &Settings::fBenchCostMult, nullptr, BENCH, {}, BENCH_EXCEPTIONS },
		{ &Settings::iFreeMendAbove, &Settings::fBenchCostMult, BENCH },
		{ &Settings::fTraderPriceMult, &Settings::bVendorRepair, TRADER_REPAIRS, TRADER_PRICE_SIDE },
		// The 2 rows that draw the bars share bHudCondition, so they never
		// run once HUD condition bars is left to another mod.
		{ &Settings::fHudBarX, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::fHudBarY, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::fPowerArmorBarX, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::fPowerArmorBarY, &Settings::bHudCondition, HUD_BARS },
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
					if (piece == kNone || piece == kMeleeWear) {
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
